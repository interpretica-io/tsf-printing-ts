/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Suite helpers
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_LGR_USER "TSAPI CYBERSEC"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>

#include "logger_api.h"
#include "tapi_cfg_base.h"
#include "tapi_job_factory_rpc.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_vector.h"

#include "tapi_devtool_run.h"
#include "tsapi_cybersec.h"

/* See description in tsapi_cybersec.h */
te_errno
tsapi_cybersec_session_init(tsapi_cybersec_session *session, const char *name)
{
    te_errno rc;

    memset(session, 0, sizeof(*session));
    session->ta = TSAPI_CYBERSEC_TA;

    rc = rcf_rpc_server_create(session->ta, name, &session->pco);
    if (rc != 0)
    {
        ERROR("Cannot create the RPC server '%s' on %s: %r", name,
              session->ta, rc);
        return rc;
    }

    /*
     * Every library in this suite reads what a command printed, which
     * needs output channels, and only the RPC factory has them.
     */
    rc = tapi_job_factory_rpc_create(session->pco, &session->factory);
    if (rc != 0)
    {
        ERROR("Cannot create a job factory over '%s': %r", name, rc);
        return rc;
    }

    /* Jobs do not inherit the agent's environment, so PATH is set here. */
    rc = tapi_job_factory_set_path(session->factory);
    if (rc != 0)
        WARN("Cannot set PATH in the job factory: %r", rc);

    return 0;
}

/* See description in tsapi_cybersec.h */
void
tsapi_cybersec_session_fini(tsapi_cybersec_session *session)
{
    if (session->factory != NULL)
    {
        tapi_job_factory_destroy(session->factory);
        session->factory = NULL;
    }

    if (session->pco != NULL)
    {
        rcf_rpc_server_destroy(session->pco);
        session->pco = NULL;
    }
}

/** Arguments of a command. */
typedef struct tsapi_args {
    size_t n_args;
    const char **args;
} tsapi_args;

static const tapi_job_opt_bind tsapi_args_binds[] = TAPI_JOB_OPT_SET(
    TAPI_JOB_OPT_ARRAY_PTR(tsapi_args, n_args, args,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, NULL, false))
);

/** Run a command on the agent and say whether it succeeded. */
static te_errno
tsapi_run(tsapi_cybersec_session *session, const char *program,
          const char **args, size_t n_args, bool *ok)
{
    tsapi_args opt = { .n_args = n_args, .args = args };
    tapi_devtool_output output;
    tapi_devtool_run run = TAPI_DEVTOOL_RUN_INIT;
    te_errno rc;

    rc = tapi_devtool_run_init(&run, session->factory, program, program,
                               tsapi_args_binds, &opt, NULL);
    if (rc != 0)
    {
        *ok = false;
        return 0;
    }

    rc = tapi_devtool_run_start(&run);
    if (rc == 0)
        rc = tapi_devtool_run_wait(&run, 30000);

    if (rc == 0)
    {
        tapi_devtool_run_get_output(&run, &output);
        *ok = output.status.type == TAPI_JOB_STATUS_EXITED &&
              output.status.value == 0;
    }
    else
    {
        *ok = false;
        rc = 0;
    }

    tapi_devtool_run_fini(&run);

    return rc;
}

/* See description in tsapi_cybersec.h */
te_errno
tsapi_cybersec_mkdir(tsapi_cybersec_session *session, const char *path)
{
    const char *args[] = { "-p", path };
    bool ok = false;
    te_errno rc;

    rc = tsapi_run(session, "mkdir", args, TE_ARRAY_LEN(args), &ok);
    if (rc != 0)
        return rc;

    if (!ok)
    {
        ERROR("Cannot make the directory '%s' on %s", path, session->ta);
        return TE_RC(TE_TAPI, TE_EFAIL);
    }

    return 0;
}

/* See description in tsapi_cybersec.h */
te_errno
tsapi_cybersec_scratch(tsapi_cybersec_session *session, const char *name,
                       te_string *path)
{
    char *tmp_dir;
    te_errno rc;

    tmp_dir = tapi_cfg_base_get_ta_dir(session->ta,
                                       TAPI_CFG_BASE_TA_DIR_TMP);
    if (tmp_dir == NULL)
    {
        ERROR("Cannot get the temporary directory of %s", session->ta);
        return TE_RC(TE_TAPI, TE_EFAIL);
    }

    te_string_append(path, "%s/tsf-%s-%u", tmp_dir, name,
                     (unsigned int)getpid());
    free(tmp_dir);

    rc = tsapi_cybersec_mkdir(session, path->ptr);
    if (rc != 0)
        te_string_reset(path);

    return rc;
}

/* See description in tsapi_cybersec.h */
bool
tsapi_cybersec_have(tsapi_cybersec_session *session, const char *program)
{
    const char *args[] = { program };
    bool ok = false;

    /*
     * "which" rather than running the program: a tool that exists but
     * needs arguments would otherwise look like a tool that is missing.
     */
    if (tsapi_run(session, "which", args, TE_ARRAY_LEN(args), &ok) != 0)
        return false;

    if (!ok)
        RING("'%s' is not installed on %s", program, session->ta);

    return ok;
}

/* See description in tsapi_cybersec.h */
bool
tsapi_cybersec_report_has(const tapi_cybersec_report *report,
                          const char *check)
{
    const tapi_cybersec_finding *finding;

    TE_VEC_FOREACH(&report->findings, finding)
    {
        if (strcmp(finding->check, check) == 0)
            return true;
    }

    return false;
}

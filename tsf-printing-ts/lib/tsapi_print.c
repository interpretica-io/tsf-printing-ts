/** @file
 * @brief A printer of the suite's own
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_LGR_USER "TSAPI PRINT"

#include "te_config.h"

#include <signal.h>
#include <string.h>

#include "logger_api.h"
#include "te_sleep.h"
#include "tapi_job_opt.h"

#include "tapi_print.h"
#include "tsapi_print.h"

typedef struct vprinter_opt {
    unsigned int port;
    tapi_job_opt_uint_t ppm;
    const char *spool;
    const char *name;
} vprinter_opt;

static const tapi_job_opt_bind vprinter_binds[] = TAPI_JOB_OPT_SET(
    TAPI_JOB_OPT_DUMMY("-2"),
    TAPI_JOB_OPT_DUMMY("-k"),
    TAPI_JOB_OPT_STRING("-d", false, vprinter_opt, spool),
    TAPI_JOB_OPT_UINT("-p", false, NULL, vprinter_opt, port),
    TAPI_JOB_OPT_UINT_T("-s", false, NULL, vprinter_opt, ppm),
    TAPI_JOB_OPT_DUMMY("-n"),
    TAPI_JOB_OPT_DUMMY("localhost"),
    TAPI_JOB_OPT_DUMMY("-r"),
    TAPI_JOB_OPT_DUMMY("off"),
    TAPI_JOB_OPT_DUMMY("-f"),
    TAPI_JOB_OPT_DUMMY("application/pdf,text/plain,image/pwg-raster,"
                       "application/octet-stream"),
    TAPI_JOB_OPT_STRING(NULL, false, vprinter_opt, name)
);

te_errno
tsapi_print_vprinter_start(tapi_job_factory_t *factory, unsigned int port,
                           const char *spool, unsigned int ppm,
                           tsapi_print_vprinter *printer)
{
    vprinter_opt opt = { .port = port, .ppm = ppm != 0 ? TAPI_JOB_OPT_UINT_VAL(ppm) :
                                            TAPI_JOB_OPT_UINT_UNDEF,
                         .spool = spool,
                         .name = NULL };
    te_string name = TE_STRING_INIT;
    unsigned int attempt;
    te_errno rc;

    printer->run = (tapi_devtool_run)TAPI_DEVTOOL_RUN_INIT;
    printer->uri = (te_string)TE_STRING_INIT;

    te_string_append(&name, "TSF Virtual %u", port);
    opt.name = name.ptr;

    rc = tapi_devtool_run_init(&printer->run, factory, "ippeveprinter",
                               "ippeveprinter", vprinter_binds, &opt, NULL);
    if (rc == 0)
        rc = tapi_devtool_run_start(&printer->run);
    te_string_free(&name);
    if (rc != 0)
        return rc;

    te_string_append(&printer->uri, "ipp://localhost:%u/ipp/print", port);

    /* It is a process long before it is a printer. */
    for (attempt = 0; attempt < 40; attempt++)
    {
        tapi_print_printer p;

        /* A printer that has exited will not start answering. */
        rc = tapi_devtool_run_wait(&printer->run, 0);
        if (TE_RC_GET_ERROR(rc) != TE_EINPROGRESS)
        {
            ERROR("ippeveprinter exited before it answered: %s",
                  te_string_value(&printer->run.err));
            tsapi_print_vprinter_stop(printer);
            return TE_RC(TE_TAPI, TE_ESRCH);
        }

        if (tapi_print_printer_get(factory, TAPI_PRINT_IPP,
                                   printer->uri.ptr, 5000, &p) == 0)
        {
            tapi_print_printer_log(&p);
            tapi_print_printer_free(&p);
            return 0;
        }
        te_msleep(250);
    }

    ERROR("The virtual printer never answered on %s", printer->uri.ptr);
    tsapi_print_vprinter_stop(printer);

    return TE_RC(TE_TAPI, TE_ETIMEDOUT);
}

void
tsapi_print_vprinter_stop(tsapi_print_vprinter *printer)
{
    if (printer->run.job != NULL)
    {
        tapi_devtool_run_kill(&printer->run, SIGTERM);
        tapi_devtool_run_wait(&printer->run, 5000);
        tapi_devtool_run_fini(&printer->run);
    }
    te_string_free(&printer->uri);
}

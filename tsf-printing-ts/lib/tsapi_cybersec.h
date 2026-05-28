/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Suite helpers
 *
 * Every test here needs the same three things: an agent, an RPC server
 * on it, and a job factory over that server. This makes them once.
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#ifndef __TSAPI_CYBERSEC_H__
#define __TSAPI_CYBERSEC_H__

#include "rcf_rpc.h"
#include "tapi_job.h"
#include "te_errno.h"
#include "te_string.h"

#include "tapi_cybersec.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The agent this suite works on. */
#define TSAPI_CYBERSEC_TA   "Agt_A"

/** What a test needs to talk to the agent. */
typedef struct tsapi_cybersec_session {
    /** Agent name. */
    const char *ta;
    /** RPC server on it. */
    rcf_rpc_server *pco;
    /** Job factory over that RPC server. */
    tapi_job_factory_t *factory;
} tsapi_cybersec_session;

/**
 * Open a session: an RPC server on the agent and a job factory over it.
 *
 * @param[out] session  Session.
 * @param[in]  name     Name for the RPC server, unique within the test.
 *
 * @return Status code.
 */
extern te_errno tsapi_cybersec_session_init(tsapi_cybersec_session *session,
                                            const char *name);

/**
 * Close a session.
 *
 * @param session       Session.
 */
extern void tsapi_cybersec_session_fini(tsapi_cybersec_session *session);

/**
 * Make a directory on the agent, parents included.
 *
 * @param session       Session.
 * @param path          Path on the agent.
 *
 * @return Status code.
 */
extern te_errno tsapi_cybersec_mkdir(tsapi_cybersec_session *session,
                                     const char *path);

/**
 * Make a scratch directory of this test's own on the agent.
 *
 * @param[in]  session  Session.
 * @param[in]  name     Something to tell it apart by.
 * @param[out] path     String to append the path to.
 *
 * @return Status code.
 */
extern te_errno tsapi_cybersec_scratch(tsapi_cybersec_session *session,
                                       const char *name, te_string *path);

/**
 * Check whether a program is on the agent at all.
 *
 * @param session       Session.
 * @param program       Program name.
 *
 * @return @c true if it can be run.
 */
extern bool tsapi_cybersec_have(tsapi_cybersec_session *session,
                                const char *program);

/**
 * Log a report and fail the test when it holds a finding that matters.
 *
 * A macro rather than a function, because the verdict has to be emitted
 * from the test's own scope: @c TEST_VERDICT() jumps to @c cleanup.
 */
#define TSAPI_CYBERSEC_CHECK(report_, min_)                                 \
    do {                                                                    \
        te_string tsapi_verdict_ = TE_STRING_INIT;                          \
                                                                            \
        tapi_cybersec_report_log(report_);                                  \
        if (tapi_cybersec_report_verdict((report_), (min_),                 \
                                         &tsapi_verdict_))                  \
        {                                                                   \
            char *tsapi_text_ = tsapi_verdict_.ptr;                         \
                                                                            \
            TEST_VERDICT("%s", tsapi_text_);                                \
        }                                                                   \
        te_string_free(&tsapi_verdict_);                                    \
    } while (0)

/**
 * Fail the test unless the report holds a finding with this check name.
 *
 * The tests here plant a defect and then ask a scanner to find it, so
 * "the finding is there" is the assertion, not "there are no findings".
 */
#define TSAPI_CYBERSEC_EXPECT(report_, check_)                              \
    do {                                                                    \
        if (!tsapi_cybersec_report_has((report_), (check_)))                \
            TEST_VERDICT("the scanner did not report %s", (check_));        \
    } while (0)

/**
 * Check whether a report holds a finding of a given check.
 *
 * @param report        Report.
 * @param check         Check identifier, e.g. @c "binary.no-relro".
 *
 * @return @c true if the report holds one.
 */
extern bool tsapi_cybersec_report_has(const tapi_cybersec_report *report,
                                      const char *check);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSAPI_CYBERSEC_H__ */

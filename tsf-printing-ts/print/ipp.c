/** @file
 * @brief Print Group
 *
 * IPP straight to a printer: capabilities, validation, a job with
 * options and the printer's own view of it, hold, release, cancel, and
 * the options that must be refused before anything is printed.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "print/ipp"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tapi_test.h"
#include "te_kvpair.h"
#include "te_string.h"

#include "tapi_print.h"
#include "tapi_print_caps.h"
#include "tapi_print_ipp.h"
#include "tapi_print_job.h"
#include "tsapi_cybersec.h"
#include "tsapi_print.h"

#define T_MS 30000
#define PORT 8631

/** The printer's own attributes of one of its jobs. */
static te_errno
job_attrs(tapi_job_factory_t *factory, const char *uri, unsigned int id,
          te_kvpair_h *attrs)
{
    te_string body = TE_STRING_INIT;
    te_string status = TE_STRING_INIT;
    te_errno rc;

    te_string_append(&body,
        "OPERATION Get-Job-Attributes\n"
        "GROUP operation-attributes-tag\n"
        "ATTR charset attributes-charset utf-8\n"
        "ATTR naturalLanguage attributes-natural-language en\n"
        "ATTR uri printer-uri $uri\n"
        "ATTR integer job-id %u\n"
        "ATTR keyword requested-attributes all\n", id);
    rc = tapi_print_ipp_request(factory, uri, body.ptr, NULL, T_MS,
                                &status, attrs);
    te_string_free(&body);
    te_string_free(&status);

    return rc;
}

/** Fail unless the attribute has exactly this value. */
#define EXPECT_ATTR(attrs_, name_, value_) \
    do {                                                                \
        const char *v_ = te_kvpairs_get_nth((attrs_), (name_), 0);      \
                                                                        \
        if (v_ == NULL || strcmp(v_, (value_)) != 0)                    \
            TEST_VERDICT("The printer has %s = %s, expected %s",        \
                         (name_), v_ == NULL ? "nothing" : v_,          \
                         (value_));                                     \
    } while (0)

int
main(int argc, char **argv)
{
    tsapi_cybersec_session sess;
    tsapi_print_vprinter vp;
    bool vp_started = false;
    te_string spool = TE_STRING_INIT;
    te_string reason = TE_STRING_INIT;
    tapi_print_caps caps;
    bool have_caps = false;
    tapi_print_opts opts = TAPI_PRINT_OPTS_INIT;
    tapi_print_job job;
    te_kvpair_h extra;
    te_kvpair_h attrs;
    unsigned int id;
    unsigned int id2;

    te_kvpair_init(&extra);
    te_kvpair_init(&attrs);

    TEST_START;

    TEST_STEP("Open a session and start a virtual printer on the agent");
    CHECK_RC(tsapi_cybersec_session_init(&sess, "pco_print_ipp"));
    if (!tapi_print_available(sess.factory, TAPI_PRINT_IPP, T_MS))
        TEST_SKIP("There is no ipptool on the agent");
    CHECK_RC(tsapi_cybersec_scratch(&sess, "print-ipp", &spool));
    CHECK_RC(tsapi_print_vprinter_start(sess.factory, PORT, spool.ptr, 6, &vp));
    vp_started = true;

    TEST_STEP("It says what it supports");
    CHECK_RC(tapi_print_caps_get(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                                 T_MS, &caps));
    have_caps = true;
    tapi_print_caps_log(&caps);
    if (!caps.duplex ||
        !tapi_print_caps_has(&caps.sides, "two-sided-short-edge") ||
        !tapi_print_caps_has(&caps.media, "iso_a4_210x297mm") ||
        !tapi_print_caps_has(&caps.formats, "text/plain") ||
        caps.copies_max < 2)
    {
        TEST_VERDICT("The capabilities are not those of the printer "
                     "that was started");
    }

    TEST_STEP("It would take a job it supports");
    opts.copies = 2;
    opts.sides = TAPI_PRINT_SIDES_TWO_SHORT;
    opts.media = "iso_a4_210x297mm";
    CHECK_RC(tapi_print_validate(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                                 &opts, T_MS, NULL));

    TEST_STEP("It would refuse one it does not, and say why");
    opts = (tapi_print_opts)TAPI_PRINT_OPTS_INIT;
    te_kvpair_add(&extra, "sides", "bogus-sides");
    opts.extra = &extra;
    rc = tapi_print_validate(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                             &opts, T_MS, &reason);
    if (TE_RC_GET_ERROR(rc) != TE_EINVAL)
        TEST_VERDICT("A bogus sides value was not refused: %r", rc);
    if (reason.len == 0)
        TEST_VERDICT("The refusal came with no reason");
    RING("Refused because: %s", reason.ptr);

    TEST_STEP("Print text with options and wait for it");
    opts = (tapi_print_opts)TAPI_PRINT_OPTS_INIT;
    opts.job_name = "tsf ipp job";
    opts.copies = 2;
    opts.sides = TAPI_PRINT_SIDES_TWO_SHORT;
    opts.media = "iso_a4_210x297mm";
    opts.color = TAPI_PRINT_COLOR_MONOCHROME;
    opts.quality = TAPI_PRINT_QUALITY_HIGH;
    opts.page_ranges = "1";
    CHECK_RC(tapi_print_text(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                             "tsf-printing\nover IPP\n", &opts, T_MS, &id));
    CHECK_RC(tapi_print_job_wait(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                                 id, 60000, &job));
    if (job.state != TAPI_PRINT_JOB_COMPLETED)
        TEST_VERDICT("The job ended %s",
                     tapi_print_job_state2str(job.state));
    tapi_print_job_free(&job);

    TEST_STEP("The printer holds the options it was given");
    CHECK_RC(job_attrs(sess.factory, vp.uri.ptr, id, &attrs));
    EXPECT_ATTR(&attrs, "job-name", "tsf ipp job");
    EXPECT_ATTR(&attrs, "copies", "2");
    EXPECT_ATTR(&attrs, "sides", "two-sided-short-edge");
    EXPECT_ATTR(&attrs, "print-color-mode", "monochrome");
    EXPECT_ATTR(&attrs, "print-quality", "high");
    EXPECT_ATTR(&attrs, "page-ranges", "1-1");
    EXPECT_ATTR(&attrs, "document-format-supplied", "text/plain");

    TEST_STEP("A printer that cannot hold refuses a held job");
    /*
     * ippeveprinter lists neither Hold-Job nor job-hold-until, so the
     * capabilities must say it cannot hold, and a job submitted held
     * must be refused - not printed unheld.
     */
    if (caps.hold)
        TEST_VERDICT("The capabilities say a printer without Hold-Job "
                     "can hold");
    opts = (tapi_print_opts)TAPI_PRINT_OPTS_INIT;
    opts.hold = true;
    rc = tapi_print_text(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                         "held\n", &opts, T_MS, NULL);
    if (TE_RC_GET_ERROR(rc) != TE_EOPNOTSUPP)
        TEST_VERDICT("A held job on a printer that cannot hold gave %r", rc);

    TEST_STEP("A second job waits out a busy printer");
    /*
     * At 6 pages a minute the first job prints for about ten seconds,
     * and the printer answers the second with server-error-busy until
     * it is done. The second must go through, not fail.
     */
    opts = (tapi_print_opts)TAPI_PRINT_OPTS_INIT;
    opts.job_name = "tsf first";
    CHECK_RC(tapi_print_text(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                             "first\n", &opts, T_MS, &id));
    opts.job_name = "tsf second";
    CHECK_RC(tapi_print_text(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                             "second\n", &opts, T_MS, &id2));
    CHECK_RC(tapi_print_job_wait(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                                 id, 60000, &job));
    if (job.state != TAPI_PRINT_JOB_COMPLETED)
        TEST_VERDICT("The first job ended %s",
                     tapi_print_job_state2str(job.state));
    tapi_print_job_free(&job);
    CHECK_RC(tapi_print_job_wait(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                                 id2, 60000, &job));
    if (job.state != TAPI_PRINT_JOB_COMPLETED)
        TEST_VERDICT("The second job ended %s",
                     tapi_print_job_state2str(job.state));
    tapi_print_job_free(&job);

    TEST_STEP("A job cancelled while it prints ends cancelled");
    opts.job_name = "tsf cancel";
    CHECK_RC(tapi_print_text(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                             "cancel me\n", &opts, T_MS, &id));
    CHECK_RC(tapi_print_job_cancel(sess.factory, TAPI_PRINT_IPP,
                                   vp.uri.ptr, id, T_MS));
    CHECK_RC(tapi_print_job_wait(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                                 id, 60000, &job));
    if (job.state != TAPI_PRINT_JOB_CANCELED)
        TEST_VERDICT("A cancelled job ended %s",
                     tapi_print_job_state2str(job.state));
    tapi_print_job_free(&job);

    TEST_STEP("The finished jobs are listed");
    {
        te_vec jobs = TE_VEC_INIT(tapi_print_job);
        tapi_print_job *j;
        bool found = false;

        CHECK_RC(tapi_print_jobs(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr,
                                 true, T_MS, &jobs));
        TE_VEC_FOREACH(&jobs, j)
        {
            tapi_print_job_log(j);
            if (j->id == id && j->state == TAPI_PRINT_JOB_CANCELED)
                found = true;
        }
        tapi_print_jobs_free(&jobs);
        if (!found)
            TEST_VERDICT("The cancelled job is not among the finished");
    }

    TEST_STEP("What cannot be passed is refused before printing");
    opts = (tapi_print_opts)TAPI_PRINT_OPTS_INIT;
    opts.number_up = 2;
    rc = tapi_print_text(sess.factory, TAPI_PRINT_WINDOWS, "nowhere", "x",
                         &opts, T_MS, NULL);
    if (TE_RC_GET_ERROR(rc) != TE_EOPNOTSUPP)
        TEST_VERDICT("Pages per sheet on Windows gave %r, not EOPNOTSUPP",
                     rc);
    opts = (tapi_print_opts)TAPI_PRINT_OPTS_INIT;
    opts.raw = true;
    opts.copies = 2;
    rc = tapi_print_text(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr, "x",
                         &opts, T_MS, NULL);
    if (TE_RC_GET_ERROR(rc) != TE_EINVAL)
        TEST_VERDICT("Copies of a raw job gave %r, not EINVAL", rc);
    opts = (tapi_print_opts)TAPI_PRINT_OPTS_INIT;
    opts.page_ranges = "1-3;5";
    rc = tapi_print_text(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr, "x",
                         &opts, T_MS, NULL);
    if (TE_RC_GET_ERROR(rc) != TE_EINVAL)
        TEST_VERDICT("Malformed page ranges gave %r, not EINVAL", rc);

    TEST_SUCCESS;

cleanup:
    if (have_caps)
        tapi_print_caps_free(&caps);
    if (vp_started)
        tsapi_print_vprinter_stop(&vp);
    te_kvpair_fini(&extra);
    te_kvpair_fini(&attrs);
    te_string_free(&spool);
    te_string_free(&reason);
    tsapi_cybersec_session_fini(&sess);

    TEST_END;
}

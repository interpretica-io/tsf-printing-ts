/** @file
 * @brief Print Group
 *
 * Jobs through a CUPS queue: a file from the engine printed with a set
 * of options, what the queue and the printer each hold of them, hold,
 * release, cancel, and a paused queue.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "print/cups_jobs"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tapi_test.h"
#include "te_kvpair.h"
#include "te_string.h"
#include "tapi_file.h"

#include "tapi_print.h"
#include "tapi_print_ipp.h"
#include "tapi_print_job.h"
#include "tsapi_cybersec.h"
#include "tsapi_print.h"

#define T_MS  30000
#define PORT  8633
#define QUEUE "tsf_jq"
#define QUEUE_URI "ipp://localhost/printers/" QUEUE

/** The job attributes @p uri holds for one job. */
static te_errno
job_attrs(tapi_job_factory_t *factory, const char *uri, unsigned int id,
          te_kvpair_h *attrs)
{
    te_string body = TE_STRING_INIT;
    te_errno rc;

    te_string_append(&body,
        "OPERATION Get-Job-Attributes\n"
        "GROUP operation-attributes-tag\n"
        "ATTR charset attributes-charset utf-8\n"
        "ATTR naturalLanguage attributes-natural-language en\n"
        "ATTR uri printer-uri $uri\n"
        "ATTR integer job-id %u\n"
        "ATTR name requesting-user-name $user\n"
        "ATTR keyword requested-attributes all\n", id);
    rc = tapi_print_ipp_request(factory, uri, body.ptr, NULL, T_MS, NULL,
                                attrs);
    te_string_free(&body);

    return rc;
}

/** The value of an attribute, or "nothing". */
static const char *
attr(const te_kvpair_h *attrs, const char *name)
{
    const char *v = te_kvpairs_get_nth(attrs, name, 0);

    return v != NULL ? v : "nothing";
}

/** Wait for a job and fail unless it ended as expected. */
#define EXPECT_END(factory_, id_, expected_) \
    do {                                                                \
        tapi_print_job job_;                                            \
                                                                        \
        CHECK_RC(tapi_print_job_wait((factory_), TAPI_PRINT_CUPS, QUEUE,  \
                                     (id_), 60000, &job_));             \
        if (job_.state != (expected_))                                  \
        {                                                               \
            tapi_print_job_state s_ = job_.state;                       \
                                                                        \
            tapi_print_job_free(&job_);                                 \
            TEST_VERDICT("Job ended %s, expected %s",                   \
                         tapi_print_job_state2str(s_),                  \
                         tapi_print_job_state2str(expected_));          \
        }                                                               \
        tapi_print_job_free(&job_);                                     \
    } while (0)

/** Fail unless the job is in the expected state now. */
#define EXPECT_NOW(factory_, id_, expected_) \
    do {                                                                \
        tapi_print_job job_;                                            \
                                                                        \
        CHECK_RC(tapi_print_job_get((factory_), TAPI_PRINT_CUPS, QUEUE,   \
                                    (id_), T_MS, &job_));               \
        tapi_print_job_log(&job_);                                      \
        if (job_.state != (expected_))                                  \
        {                                                               \
            tapi_print_job_state s_ = job_.state;                       \
                                                                        \
            tapi_print_job_free(&job_);                                 \
            TEST_VERDICT("Job is %s, expected %s",                      \
                         tapi_print_job_state2str(s_),                  \
                         tapi_print_job_state2str(expected_));          \
        }                                                               \
        tapi_print_job_free(&job_);                                     \
    } while (0)

int
main(int argc, char **argv)
{
    tsapi_cybersec_session sess;
    tsapi_print_vprinter vp;
    bool vp_started = false;
    bool queue_added = false;
    te_string spool = TE_STRING_INIT;
    te_string local = TE_STRING_INIT;
    te_string remote = TE_STRING_INIT;
    tapi_print_queue_spec spec = { .name = QUEUE };
    tapi_print_opts opts = TAPI_PRINT_OPTS_INIT;
    te_kvpair_h queue_view;
    te_kvpair_h printer_view;
    unsigned int id;
    unsigned int printer_id;
    FILE *f;

    te_kvpair_init(&queue_view);
    te_kvpair_init(&printer_view);

    TEST_START;

    TEST_STEP("Open a session, start a virtual printer, add a queue");
    CHECK_RC(tsapi_cybersec_session_init(&sess, "pco_print_cups_jobs"));
    if (!tapi_print_available(sess.factory, TAPI_PRINT_IPP, T_MS))
        TEST_SKIP("There is no ipptool on the agent");
    CHECK_RC(tsapi_cybersec_scratch(&sess, "print-cups-jobs", &spool));
    CHECK_RC(tsapi_print_vprinter_start(sess.factory, PORT, spool.ptr, 0, &vp));
    vp_started = true;
    spec.device = vp.uri.ptr;
    CHECK_RC(tapi_print_queue_add(sess.factory, TAPI_PRINT_CUPS, &spec,
                                  60000));
    queue_added = true;

    TEST_STEP("Put a document from the engine on the agent");
    tapi_file_make_custom_pathname(&local, getenv("TE_TMP"), ".txt");
    f = fopen(local.ptr, "w");
    if (f == NULL)
        TEST_FAIL("Cannot write %s", local.ptr);
    fprintf(f, "tsf-printing\nfrom the engine\nthrough the agent\n");
    fclose(f);
    CHECK_RC(tapi_print_put(sess.factory, local.ptr, &remote));

    TEST_STEP("Print it with options and wait for it to complete");
    opts.job_name = "tsf cups job";
    opts.copies = 2;
    opts.sides = TAPI_PRINT_SIDES_TWO_LONG;
    opts.media = "iso_a4_210x297mm";
    opts.color = TAPI_PRINT_COLOR_MONOCHROME;
    opts.orientation = TAPI_PRINT_ORIENT_LANDSCAPE;
    opts.number_up = 2;
    opts.collate = TE_BOOL3_TRUE;
    opts.priority = 60;
    CHECK_RC(tapi_print_file(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                             remote.ptr, &opts, T_MS, &id));
    EXPECT_END(sess.factory, id, TAPI_PRINT_JOB_COMPLETED);

    TEST_STEP("The queue holds every option");
    CHECK_RC(job_attrs(sess.factory, QUEUE_URI, id, &queue_view));
    RING("Queue's view: copies=%s sides=%s media=%s print-color-mode=%s "
         "orientation-requested=%s number-up=%s job-priority=%s "
         "job-name=%s", attr(&queue_view, "copies"),
         attr(&queue_view, "sides"), attr(&queue_view, "media"),
         attr(&queue_view, "print-color-mode"),
         attr(&queue_view, "orientation-requested"),
         attr(&queue_view, "number-up"), attr(&queue_view, "job-priority"),
         attr(&queue_view, "job-name"));
    if (strcmp(attr(&queue_view, "copies"), "2") != 0 ||
        strcmp(attr(&queue_view, "sides"), "two-sided-long-edge") != 0 ||
        strcmp(attr(&queue_view, "media"), "iso_a4_210x297mm") != 0 ||
        strcmp(attr(&queue_view, "print-color-mode"), "monochrome") != 0 ||
        strcmp(attr(&queue_view, "orientation-requested"),
               "landscape") != 0 ||
        strcmp(attr(&queue_view, "number-up"), "2") != 0 ||
        strcmp(attr(&queue_view, "job-priority"), "60") != 0 ||
        strcmp(attr(&queue_view, "job-name"), "tsf cups job") != 0)
    {
        TEST_VERDICT("The queue does not hold the options it was given");
    }

    TEST_STEP("The printer holds what CUPS passed on");
    /*
     * The virtual printer numbers its own jobs, and this is its first
     * one: the queue was made for this test.
     */
    printer_id = 1;
    CHECK_RC(job_attrs(sess.factory, vp.uri.ptr, printer_id,
                       &printer_view));
    RING("Printer's view: copies=%s sides=%s media-col=%s "
         "print-color-mode=%s orientation-requested=%s number-up=%s "
         "job-name=%s document-format=%s",
         attr(&printer_view, "copies"), attr(&printer_view, "sides"),
         attr(&printer_view, "media-col"),
         attr(&printer_view, "print-color-mode"),
         attr(&printer_view, "orientation-requested"),
         attr(&printer_view, "number-up"), attr(&printer_view, "job-name"),
         attr(&printer_view, "document-format-supplied"));
    if (strcmp(attr(&printer_view, "sides"), "two-sided-long-edge") != 0 ||
        strcmp(attr(&printer_view, "print-color-mode"), "monochrome") != 0)
    {
        TEST_VERDICT("The printer did not get the sides and colour mode");
    }

    TEST_STEP("A job submitted held is held, and completes once released");
    opts = (tapi_print_opts)TAPI_PRINT_OPTS_INIT;
    opts.hold = true;
    CHECK_RC(tapi_print_file(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                             remote.ptr, &opts, T_MS, &id));
    EXPECT_NOW(sess.factory, id, TAPI_PRINT_JOB_HELD);
    CHECK_RC(tapi_print_job_release(sess.factory, TAPI_PRINT_CUPS, QUEUE, id,
                                    T_MS));
    EXPECT_END(sess.factory, id, TAPI_PRINT_JOB_COMPLETED);

    TEST_STEP("A held job that is cancelled ends cancelled, not completed");
    CHECK_RC(tapi_print_file(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                             remote.ptr, &opts, T_MS, &id));
    CHECK_RC(tapi_print_job_cancel(sess.factory, TAPI_PRINT_CUPS, QUEUE, id,
                                   T_MS));
    EXPECT_END(sess.factory, id, TAPI_PRINT_JOB_CANCELED);

    TEST_STEP("A pending job can be held after it was submitted");
    CHECK_RC(tapi_print_queue_enable(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                     false, "tsf paused", T_MS));
    opts = (tapi_print_opts)TAPI_PRINT_OPTS_INIT;
    CHECK_RC(tapi_print_text(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                             "waiting\n", &opts, T_MS, &id));
    EXPECT_NOW(sess.factory, id, TAPI_PRINT_JOB_PENDING);
    CHECK_RC(tapi_print_job_hold(sess.factory, TAPI_PRINT_CUPS, QUEUE, id,
                                 T_MS));
    EXPECT_NOW(sess.factory, id, TAPI_PRINT_JOB_HELD);

    TEST_STEP("Cancelling everything on the queue cancels it");
    CHECK_RC(tapi_print_cancel_all(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                   T_MS));
    EXPECT_END(sess.factory, id, TAPI_PRINT_JOB_CANCELED);
    CHECK_RC(tapi_print_queue_enable(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                     true, NULL, T_MS));

    TEST_STEP("Nothing is left unfinished");
    {
        te_vec jobs = TE_VEC_INIT(tapi_print_job);

        CHECK_RC(tapi_print_jobs(sess.factory, TAPI_PRINT_CUPS, QUEUE, false,
                                 T_MS, &jobs));
        if (te_vec_size(&jobs) != 0)
        {
            tapi_print_jobs_free(&jobs);
            TEST_VERDICT("Jobs are left on the queue");
        }
        tapi_print_jobs_free(&jobs);
    }

    TEST_SUCCESS;

cleanup:
    if (queue_added)
        tapi_print_queue_del(sess.factory, TAPI_PRINT_CUPS, QUEUE, T_MS);
    if (vp_started)
        tsapi_print_vprinter_stop(&vp);
    if (local.len != 0)
        unlink(local.ptr);
    te_kvpair_fini(&queue_view);
    te_kvpair_fini(&printer_view);
    te_string_free(&spool);
    te_string_free(&local);
    te_string_free(&remote);
    tsapi_cybersec_session_fini(&sess);

    TEST_END;
}

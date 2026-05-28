/** @file
 * @brief Print Group
 *
 * A CUPS queue from start to end: add, read, make default, pause,
 * reject, capabilities, validation, remove - and printing to a queue
 * that is gone.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "print/cups_queue"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tapi_test.h"
#include "te_string.h"

#include "tapi_print.h"
#include "tapi_print_caps.h"
#include "tapi_print_job.h"
#include "tsapi_cybersec.h"
#include "tsapi_print.h"

#define T_MS  30000
#define PORT  8632
#define QUEUE "tsf_q"

int
main(int argc, char **argv)
{
    tsapi_cybersec_session sess;
    tsapi_print_vprinter vp;
    bool vp_started = false;
    bool queue_added = false;
    te_string spool = TE_STRING_INIT;
    te_string old_default = TE_STRING_INIT;
    te_string name = TE_STRING_INIT;
    tapi_print_queue_spec spec = { .name = QUEUE };
    tapi_print_printer printer;
    tapi_print_caps caps;
    tapi_print_opts opts = TAPI_PRINT_OPTS_INIT;

    TEST_START;

    TEST_STEP("Open a session and start a virtual printer on the agent");
    CHECK_RC(tsapi_cybersec_session_init(&sess, "pco_print_cups_queue"));
    CHECK_RC(tsapi_cybersec_scratch(&sess, "print-cups-queue", &spool));
    CHECK_RC(tsapi_print_vprinter_start(sess.factory, PORT, spool.ptr, 0, &vp));
    vp_started = true;

    TEST_STEP("Add an IPP Everywhere queue in front of it");
    spec.device = vp.uri.ptr;
    spec.location = "bench 3";
    spec.info = "TSF queue";
    spec.shared = false;
    CHECK_RC(tapi_print_queue_add(sess.factory, TAPI_PRINT_CUPS, &spec,
                                  60000));
    queue_added = true;

    TEST_STEP("The queue is what was asked for, enabled and accepting");
    CHECK_RC(tapi_print_printer_get(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                    T_MS, &printer));
    tapi_print_printer_log(&printer);
    if (printer.state != TAPI_PRINT_PRINTER_IDLE || !printer.accepting)
        TEST_VERDICT("A new queue is %s and %saccepting",
                     tapi_print_printer_state2str(printer.state),
                     printer.accepting ? "" : "not ");
    if (printer.shared)
        TEST_VERDICT("A queue added unshared is shared");
    if (printer.device == NULL || strcmp(printer.device, vp.uri.ptr) != 0 ||
        printer.info == NULL || strcmp(printer.info, "TSF queue") != 0 ||
        printer.location == NULL || strcmp(printer.location, "bench 3") != 0)
    {
        TEST_VERDICT("The queue's device, description or location is "
                     "not what was set");
    }
    tapi_print_printer_free(&printer);

    TEST_STEP("Make it the default, and read that back");
    rc = tapi_print_default_get(sess.factory, TAPI_PRINT_CUPS, T_MS,
                                &old_default);
    if (rc != 0 && TE_RC_GET_ERROR(rc) != TE_ENOENT)
        TEST_VERDICT("Reading the default failed: %r", rc);
    CHECK_RC(tapi_print_default_set(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                    T_MS));
    CHECK_RC(tapi_print_default_get(sess.factory, TAPI_PRINT_CUPS, T_MS,
                                    &name));
    if (strcmp(name.ptr, QUEUE) != 0)
        TEST_VERDICT("The default is %s after setting it", name.ptr);

    TEST_STEP("A paused queue is stopped, and says why");
    CHECK_RC(tapi_print_queue_enable(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                     false, "tsf maintenance", T_MS));
    CHECK_RC(tapi_print_printer_get(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                    T_MS, &printer));
    tapi_print_printer_log(&printer);
    if (printer.state != TAPI_PRINT_PRINTER_STOPPED)
        TEST_VERDICT("A paused queue is %s",
                     tapi_print_printer_state2str(printer.state));
    if (printer.message == NULL ||
        strstr(printer.message, "tsf maintenance") == NULL)
        TEST_VERDICT("A paused queue does not say why");
    tapi_print_printer_free(&printer);
    CHECK_RC(tapi_print_queue_enable(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                     true, NULL, T_MS));

    TEST_STEP("A rejecting queue refuses a job with EPERM");
    CHECK_RC(tapi_print_queue_accept(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                     false, "tsf closed", T_MS));
    CHECK_RC(tapi_print_printer_get(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                    T_MS, &printer));
    if (printer.accepting)
        TEST_VERDICT("A rejecting queue says it accepts");
    tapi_print_printer_free(&printer);
    rc = tapi_print_text(sess.factory, TAPI_PRINT_CUPS, QUEUE, "no\n", NULL,
                         T_MS, NULL);
    if (TE_RC_GET_ERROR(rc) != TE_EPERM)
        TEST_VERDICT("Printing to a rejecting queue gave %r", rc);
    CHECK_RC(tapi_print_queue_accept(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                     true, NULL, T_MS));

    TEST_STEP("The queue reports the printer's capabilities");
    CHECK_RC(tapi_print_caps_get(sess.factory, TAPI_PRINT_CUPS, QUEUE, T_MS,
                                 &caps));
    tapi_print_caps_log(&caps);
    if (!caps.duplex || te_vec_size(&caps.media) == 0)
    {
        tapi_print_caps_free(&caps);
        TEST_VERDICT("The queue does not report a two-sided printer "
                     "with media");
    }
    tapi_print_caps_free(&caps);

    TEST_STEP("The queue validates a job without printing it");
    opts.copies = 3;
    opts.sides = TAPI_PRINT_SIDES_TWO_LONG;
    CHECK_RC(tapi_print_validate(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                 &opts, T_MS, NULL));

    TEST_STEP("Remove it; it is gone, and printing to it says so");
    CHECK_RC(tapi_print_queue_del(sess.factory, TAPI_PRINT_CUPS, QUEUE,
                                  T_MS));
    queue_added = false;
    rc = tapi_print_printer_get(sess.factory, TAPI_PRINT_CUPS, QUEUE, T_MS,
                                &printer);
    if (TE_RC_GET_ERROR(rc) != TE_ENOENT)
        TEST_VERDICT("A removed queue gave %r, not ENOENT", rc);
    rc = tapi_print_text(sess.factory, TAPI_PRINT_CUPS, QUEUE, "gone\n",
                         NULL, T_MS, NULL);
    if (TE_RC_GET_ERROR(rc) != TE_ENOENT)
        TEST_VERDICT("Printing to a removed queue gave %r, not ENOENT", rc);

    TEST_SUCCESS;

cleanup:
    if (old_default.len != 0)
        tapi_print_default_set(sess.factory, TAPI_PRINT_CUPS,
                               old_default.ptr, T_MS);
    if (queue_added)
        tapi_print_queue_del(sess.factory, TAPI_PRINT_CUPS, QUEUE, T_MS);
    if (vp_started)
        tsapi_print_vprinter_stop(&vp);
    te_string_free(&spool);
    te_string_free(&old_default);
    te_string_free(&name);
    tsapi_cybersec_session_fini(&sess);

    TEST_END;
}

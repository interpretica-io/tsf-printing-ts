/** @file
 * @brief Print Group
 *
 * The posture of a print system: a queue planted to send in clear text
 * to another host must be reported, and a printer's IPP security must
 * be reported exactly as the printer describes it.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "print/audit"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tapi_test.h"
#include "te_kvpair.h"
#include "te_string.h"

#include "tapi_print.h"
#include "tapi_print_audit.h"
#include "tapi_print_ipp.h"
#include "tsapi_cybersec.h"
#include "tsapi_print.h"

#define T_MS  30000
#define PORT  8634
#define QUEUE "tsf_aq"

int
main(int argc, char **argv)
{
    tsapi_cybersec_session sess;
    tsapi_print_vprinter vp;
    bool vp_started = false;
    bool queue_added = false;
    te_string spool = TE_STRING_INIT;
    tapi_print_queue_spec spec = { .name = QUEUE };
    tapi_cybersec_report cups_report;
    tapi_cybersec_report ipp_report;
    te_kvpair_h attrs;
    const char *security;
    bool offers_tls;

    tapi_cybersec_report_init(&cups_report);
    tapi_cybersec_report_init(&ipp_report);
    te_kvpair_init(&attrs);

    TEST_START;

    TEST_STEP("Open a session");
    CHECK_RC(tsapi_cybersec_session_init(&sess, "pco_print_audit"));

    TEST_STEP("Plant a queue that sends in clear text to another host");
    /*
     * 192.0.2.1 is TEST-NET-1: nothing answers there, and nothing is
     * sent there either - the queue is only looked at. A raw queue,
     * because an IPP Everywhere one would ask the printer for its
     * attributes first.
     */
    spec.device = "socket://192.0.2.1:9100";
    spec.model = "raw";
    CHECK_RC(tapi_print_queue_add(sess.factory, TAPI_PRINT_CUPS, &spec,
                                  T_MS));
    queue_added = true;

    TEST_STEP("The audit reports it");
    CHECK_RC(tapi_print_audit(sess.factory, TAPI_PRINT_CUPS, NULL, NULL, T_MS,
                              &cups_report));
    tapi_cybersec_report_log(&cups_report);
    TSAPI_CYBERSEC_EXPECT(&cups_report, "print.cleartext-device");

    TEST_STEP("Start a virtual printer and audit it over IPP");
    if (!tapi_print_available(sess.factory, TAPI_PRINT_IPP, T_MS))
        TEST_SKIP("There is no ipptool on the agent");
    CHECK_RC(tsapi_cybersec_scratch(&sess, "print-audit", &spool));
    CHECK_RC(tsapi_print_vprinter_start(sess.factory, PORT, spool.ptr, 0, &vp));
    vp_started = true;
    CHECK_RC(tapi_print_audit(sess.factory, TAPI_PRINT_IPP, vp.uri.ptr, NULL,
                              T_MS, &ipp_report));
    tapi_cybersec_report_log(&ipp_report);

    TEST_STEP("It takes jobs from anyone, and the audit says so");
    TSAPI_CYBERSEC_EXPECT(&ipp_report, "print.ipp-no-auth");

    TEST_STEP("The TLS finding agrees with what the printer offers");
    CHECK_RC(tapi_print_ipp_attrs(sess.factory, vp.uri.ptr, T_MS, &attrs));
    security = te_kvpairs_get_nth(&attrs, "uri-security-supported", 0);
    if (security == NULL)
        TEST_VERDICT("The printer does not report uri-security-supported");
    offers_tls = strstr(security, "tls") != NULL;
    RING("uri-security-supported = %s", security);
    if (offers_tls == tsapi_cybersec_report_has(&ipp_report,
                                                "print.ipp-no-tls"))
    {
        TEST_VERDICT("print.ipp-no-tls is %s for a printer that says %s",
                     offers_tls ? "reported" : "missing", security);
    }

    TEST_SUCCESS;

cleanup:
    if (queue_added)
        tapi_print_queue_del(sess.factory, TAPI_PRINT_CUPS, QUEUE, T_MS);
    if (vp_started)
        tsapi_print_vprinter_stop(&vp);
    tapi_cybersec_report_free(&cups_report);
    tapi_cybersec_report_free(&ipp_report);
    te_kvpair_fini(&attrs);
    te_string_free(&spool);
    tsapi_cybersec_session_fini(&sess);

    TEST_END;
}

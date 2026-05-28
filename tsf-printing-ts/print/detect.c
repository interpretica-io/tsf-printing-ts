/** @file
 * @brief Print Group
 *
 * Which print system the agent has, and what it holds.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "print/detect"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"

#include "tapi_print.h"
#include "tsapi_cybersec.h"

#define T_MS 30000

int
main(int argc, char **argv)
{
    tsapi_cybersec_session sess;
    tapi_print_backend backend;
    te_vec printers = TE_VEC_INIT(tapi_print_printer);
    te_string def = TE_STRING_INIT;
    tapi_print_printer *p;

    TEST_START;

    TEST_STEP("Open a session to the agent");
    CHECK_RC(tsapi_cybersec_session_init(&sess, "pco_print_detect"));

    TEST_STEP("A print system is running");
    if (!tapi_print_available(sess.factory, TAPI_PRINT_AUTO, T_MS))
        TEST_VERDICT("No print system is running on the agent");

    TEST_STEP("It is CUPS, and not the Windows spooler");
    CHECK_RC(tapi_print_detect(sess.factory, T_MS, &backend));
    if (backend != TAPI_PRINT_CUPS)
        TEST_VERDICT("A Linux agent was detected as %s",
                     tapi_print_backend2str(backend));
    if (tapi_print_available(sess.factory, TAPI_PRINT_WINDOWS, T_MS))
        TEST_VERDICT("A Windows spooler was found on a Linux agent");

    TEST_STEP("ipptool is there, so IPP and exact job states are");
    if (!tapi_print_available(sess.factory, TAPI_PRINT_IPP, T_MS))
        TEST_VERDICT("There is no ipptool on the agent");

    TEST_STEP("The capability table says what it should");
    if (!tapi_print_supports(TAPI_PRINT_CUPS, TAPI_PRINT_FEAT_REJECT) ||
        tapi_print_supports(TAPI_PRINT_WINDOWS, TAPI_PRINT_FEAT_REJECT) ||
        tapi_print_supports(TAPI_PRINT_WINDOWS, TAPI_PRINT_FEAT_FILE) ||
        tapi_print_supports(TAPI_PRINT_IPP, TAPI_PRINT_FEAT_ADMIN))
    {
        TEST_VERDICT("The capability table is wrong");
    }

    TEST_STEP("List the printers");
    CHECK_RC(tapi_print_printers(sess.factory, backend, T_MS, &printers));
    RING("%u printers", (unsigned int)te_vec_size(&printers));
    TE_VEC_FOREACH(&printers, p)
        tapi_print_printer_log(p);

    TEST_STEP("Read the default, which may be none");
    rc = tapi_print_default_get(sess.factory, backend, T_MS, &def);
    if (rc == 0)
        RING("Default printer: %s", def.ptr);
    else if (TE_RC_GET_ERROR(rc) != TE_ENOENT)
        TEST_VERDICT("Reading the default failed: %r", rc);

    TEST_SUCCESS;

cleanup:
    tapi_print_printers_free(&printers);
    te_string_free(&def);
    tsapi_cybersec_session_fini(&sess);

    TEST_END;
}

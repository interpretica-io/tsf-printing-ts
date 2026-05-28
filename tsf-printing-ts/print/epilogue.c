/** @file
 * @brief print Group
 *
 * print group epilogue.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_TEST_NAME    "print/epilogue"

#include "te_config.h"
#include "tapi_test.h"
#include "tsapi_evo.h"

int
main(int argc, char **argv)
{
    TEST_START;

    tsapi_evo_analysis_hint("print group epilogue.");

    TEST_STEP("Finalize the print group");
    /* Nothing is left listening: each test stops its own virtual printer in
     * its own cleanup, including when it failed. */

    TEST_SUCCESS;

cleanup:

    TEST_END;
}

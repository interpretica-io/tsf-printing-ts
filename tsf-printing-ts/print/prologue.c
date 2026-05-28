/** @file
 * @brief print Group
 *
 * print group prologue.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_TEST_NAME    "print/prologue"

#include "te_config.h"
#include "tapi_test.h"
#include "tsapi_evo.h"

int
main(int argc, char **argv)
{
    TEST_START;

    tsapi_evo_analysis_hint("print group prologue.");

    TEST_STEP("Prepare the print group");
    /* Each test stands up its own virtual printer on its own port and takes
     * it down again, so there is nothing to set up here. */

    TEST_SUCCESS;

cleanup:

    TEST_END;
}

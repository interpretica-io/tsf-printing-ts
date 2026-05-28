/** @file
 * @brief A printer of the suite's own
 *
 * A virtual IPP printer - ippeveprinter - started on the agent through
 * the job factory, so that there is something to print to that keeps
 * what it was sent and says what it was asked to do.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#ifndef __TSAPI_PRINT_H__
#define __TSAPI_PRINT_H__

#include "te_errno.h"
#include "te_string.h"
#include "tapi_job.h"
#include "tapi_devtool_run.h"

/** A virtual printer. */
typedef struct tsapi_print_vprinter {
    /** The ippeveprinter process. */
    tapi_devtool_run run;
    /** Its URI, as the agent reaches it. */
    te_string uri;
} tsapi_print_vprinter;

/**
 * Start a two-sided virtual printer on the agent and wait until it
 * answers IPP.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  port     TCP port.
 * @param[in]  spool    Directory on the agent to keep jobs in.
 * @param[in]  ppm      Pages per minute it pretends to print at, so a
 *                      job lasts long enough to be caught; 0 for the
 *                      default.
 * @param[out] printer  The printer.
 *
 * @return Status code.
 */
extern te_errno tsapi_print_vprinter_start(tapi_job_factory_t *factory,
                                           unsigned int port,
                                           const char *spool,
                                           unsigned int ppm,
                                           tsapi_print_vprinter *printer);

/**
 * Stop it.
 *
 * @param printer   The printer.
 */
extern void tsapi_print_vprinter_stop(tsapi_print_vprinter *printer);

#endif /* !__TSAPI_PRINT_H__ */

/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2024      Huawei Technologies Co., Ltd.
 *                         All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 * These symbols are in a file by themselves to provide nice linker
 * semantics. Since linkers generally pull in symbols by object
 * files, keeping these symbols as the only symbols in this file
 * prevents utility programs such as "ompi_info" from having to import
 * entire component just to query their version and paramters.
*/

#include "prte_config.h"
#include "constants.h"

#include "src/util/proc_info.h"
#include "src/runtime/prte_globals.h"
#include "src/mca/ess/ess.h"
#include "src/mca/ess/donau/ess_donau.h"
#include <stdlib.h>

extern prte_ess_base_module_t prte_ess_donau_module;

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */

prte_ess_base_component_t prte_mca_ess_donau_component = {
    PRTE_ESS_BASE_VERSION_3_0_0,

    /* Component name and version */
    .pmix_mca_component_name = "donau",
    PMIX_MCA_BASE_MAKE_VERSION(component, 
                                PRTE_MAJOR_VERSION, 
                                PRTE_MINOR_VERSION,
                                PRTE_RELEASE_VERSION),

    /* Component open and close functions */
    .pmix_mca_open_component = prte_ess_donau_component_open,
    .pmix_mca_close_component = prte_ess_donau_component_close,
    .pmix_mca_query_component = prte_ess_donau_component_query,
};

int prte_ess_donau_component_open(void)
{
    return PRTE_SUCCESS;
}

int prte_ess_donau_component_query(pmix_mca_base_module_t **module, int *priority)
{
    /* Are we running under a DONAU job? Were
     * we given a path back to the HNP? If the
     * answer to both is "yes", then we were launched
     * by mpirun in a donau world, so make ourselves available
     */
    char *donau_job_id = getenv("CCS_JOB_ID");
    if (PRTE_PROC_IS_DAEMON &&
        NULL != donau_job_id &&
        0 != strlen(donau_job_id) &&
        NULL != prte_process_info.my_hnp_uri &&
        DONAU_DRUN == prte_donau_launch_type) {
        *priority = 100;
        *module = (pmix_mca_base_module_t *)&prte_ess_donau_module;
        return PRTE_SUCCESS;
    }

    /*Sadly, no */
    *priority = -1;
    *module = NULL;
    return PRTE_ERROR;
}

int prte_ess_donau_component_close(void)
{
    return PRTE_SUCCESS;
}
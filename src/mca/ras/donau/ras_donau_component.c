/*
 * Copyright (c) 2024      Huawei Technologies Co., Ltd.
 *                         All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "prte_config.h"
#include "constants.h"

#include "src/mca/ras/base/base.h"
#include "src/util/pmix_net.h"
#include "src/include/prte_socket_errno.h"

#include "src/util/name_fns.h"
#include "src/mca/errmgr/errmgr.h"
#include "src/runtime/prte_globals.h"

#include "src/mca/ras/base/ras_private.h"
#include "ras_donau.h"

/*
 * Local functions
 */
static int ras_donau_register(void);
static int ras_donau_open(void);
static int ras_donau_close(void);
static int prte_mca_ras_donau_component_query(pmix_mca_base_module_t **module, int *priority);


prte_mca_ras_donau_component_t prte_mca_ras_donau_component = {
    .super = {
            PRTE_RAS_BASE_VERSION_2_0_0,

            /* Component name and version */
            .pmix_mca_component_name = "donau",
            PMIX_MCA_BASE_MAKE_VERSION(component, 
                                        PRTE_MAJOR_VERSION, 
                                        PRTE_MINOR_VERSION,
                                        PMIX_RELEASE_VERSION),

            /* Component open and close functions */
            .pmix_mca_open_component = ras_donau_open,
            .pmix_mca_close_component = ras_donau_close,
            .pmix_mca_query_component = prte_mca_ras_donau_component_query,
            .pmix_mca_register_component_params = ras_donau_register
    }
};

static int ras_donau_register(void)
{
    pmix_mca_base_component_t *component = &prte_mca_ras_donau_component.super;

    prte_mca_ras_donau_component.param_priority = 100;
    (void) pmix_mca_base_component_var_register (component,
                                            "priority", "Priority of the donau ras component",
                                            PMIX_MCA_BASE_VAR_TYPE_INT,
                                            &prte_mca_ras_donau_component.param_priority);

    return PRTE_SUCCESS;
}
static int ras_donau_open(void)
{
    return PRTE_SUCCESS;
}

static int ras_donau_close(void)
{
    return PRTE_SUCCESS;
}

static int prte_mca_ras_donau_component_query(pmix_mca_base_module_t **module, int *priority)
{
    /* check if donau is running here */
    char *donau_job_id = getenv("CCS_JOB_ID");
    if (NULL == donau_job_id || 0 == strlen(donau_job_id) || DONAU_SSH == prte_donau_launch_type) {
        /* disqualify ourselves */
        *priority = 0;
        *module = NULL;
        return PRTE_ERROR;
    }

    PMIX_OUTPUT_VERBOSE((2, prte_ras_base_framework.framework_output,
                         "%s ras:donau: available for selection",
                         PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));
    /* since only one RM can exist on a cluster, just set
     * my priority to something - the other components won't
     * be responding anyway
     */
    *priority = prte_mca_ras_donau_component.param_priority;
    *module = (pmix_mca_base_module_t *)&prte_ras_donau_module;
    return PRTE_SUCCESS;
}
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
 * entire components just to query their version and parameters.
 */

#include "prte_config.h"
#include "constants.h"

#include "src/mca/base/pmix_mca_base_var.h"
#include "src/util/pmix_environ.h"
#include "src/util/name_fns.h"
#include "src/util/pmix_show_help.h"
#include "src/runtime/prte_globals.h"

#include "src/mca/plm/plm.h"
#include "src/mca/plm/base/plm_private.h"
#include "plm_donau.h"
#include <string.h>


/*
 * Public string showing the plm ompi_donau component version number
 */
const char *mca_plm_donau_component_version_string =
  "Open MPI donau plm MCA component version " PRTE_VERSION;


/*
 * Local functions
 */
static int plm_donau_register(void);
static int plm_donau_open(void);
static int plm_donau_close(void);
static int prte_plm_donau_component_query(pmix_mca_base_module_t **module, int *priority);


/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it.
 */

prte_mca_plm_donau_component_t prte_mca_plm_donau_component = {
    .super = {
        PRTE_PLM_BASE_VERSION_2_0_0,

        /* Component name and version */
        .pmix_mca_component_name = "donau",
        PMIX_MCA_BASE_MAKE_VERSION(component, 
                                    PRTE_MAJOR_VERSION, 
                                    PRTE_MINOR_VERSION,
                                    PRTE_RELEASE_VERSION),

        /* Component open and close functions */
        .pmix_mca_open_component = plm_donau_open,
        .pmix_mca_close_component = plm_donau_close,
        .pmix_mca_query_component = prte_plm_donau_component_query,
        .pmix_mca_register_component_params = plm_donau_register,
    }

    /* Other orte_plm_donau_component_t items -- left uninitialized
       here; will be initialized in plm_donau_open() */
};


static int plm_donau_register(void)
{
    pmix_mca_base_component_t *comp = &prte_mca_plm_donau_component.super;

    prte_mca_plm_donau_component.custom_args = NULL;
    (void) pmix_mca_base_component_var_register (comp, "args", "Custom arguments to drun",
                                            PMIX_MCA_BASE_VAR_TYPE_STRING,
                                            &prte_mca_plm_donau_component.custom_args);
    prte_mca_plm_donau_component.donau_warning_msg = true;
    (void) pmix_mca_base_component_var_register (comp, "warning", "Turn off warning message",
                                            PMIX_MCA_BASE_VAR_TYPE_BOOL,
                                            &prte_mca_plm_donau_component.donau_warning_msg);
    return PRTE_SUCCESS;
}

static int plm_donau_open(void)
{
    return PRTE_SUCCESS;
}

static int prte_plm_donau_component_query(pmix_mca_base_module_t **module, int *priority)
{
    /* Are we running under a DONAU job? */
    char *donau_job_id = getenv("CCS_JOB_ID");
    if (NULL != donau_job_id &&
        0 != strlen(donau_job_id) &&
        DONAU_DRUN == prte_donau_launch_type) {
        *priority = 100;
        PMIX_OUTPUT_VERBOSE((1, prte_plm_base_framework.framework_output,
                             "%s plm:donau: available for selection",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));

        *module = (pmix_mca_base_module_t*)&prte_plm_donau_module;
        return PRTE_SUCCESS;
    }

    /* Sadly, no */
    *module = NULL;
    return PRTE_ERROR;
}

static int plm_donau_close(void)
{
    return PRTE_SUCCESS;
}
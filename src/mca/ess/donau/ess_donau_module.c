/*
 * Copyright (c) 2024      Huawei Technologies Co., Ltd.
 *                         All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

#include "prte_config.h"
#include "constants.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#include <string.h>
#include <ctype.h>

#include "src/util/proc_info.h"
#include "src/util/pmix_show_help.h"
#include "src/mca/errmgr/errmgr.h"
#include "src/rml/rml.h"
#include "src/util/name_fns.h"
#include "src/runtime/prte_globals.h"

#include "src/mca/ess/ess.h"
#include "src/mca/ess/base/base.h"
#include "src/mca/ess/donau/ess_donau.h"

static int donau_set_name(void);
static int rte_init(void);
static int rte_finalize(void);

prte_ess_base_module_t prte_ess_donau_module = {
    rte_init,
    rte_finalize,
};

static int rte_init(void)
{
    int ret;
    char *error = NULL;

    /* run the prolog */
    if (PRTE_SUCCESS != (ret = prte_ess_base_std_prolog())) {
        error = "prte_ess_base_std_prolog";
        goto error;
    }
    /* Start by getting a unique name */
    if (PRTE_SUCCESS != (ret = donau_set_name())) {
        PRTE_ERROR_LOG(ret);
        error = "donau_set_name";
        goto error;
    }
    if (PRTE_SUCCESS != (ret = prte_ess_base_prted_setup())) {
        PRTE_ERROR_LOG(ret);
        error = "prte_ess_base_prted_setup";
        goto error;
    }
    return PRTE_SUCCESS;

error:
    if (PRTE_ERR_SILENT != ret && !prte_report_silent_errors) {
        pmix_show_help("help-prte-runtime.txt",
                       "prte_init:startup:internal-failure",
                       true, error, PRTE_ERROR_NAME(ret), ret);
    }

    return ret;
}

static int rte_finalize(void)
{
    int ret;

    if (PRTE_SUCCESS != (ret = prte_ess_base_prted_finalize())) {
        PRTE_ERROR_LOG(ret);
    }

    return PRTE_SUCCESS;
}

static int donau_set_name(void)
{
    int rc;
    pmix_rank_t vpid;
    int donau_nodeid;
    PMIX_OUTPUT_VERBOSE((1, prte_ess_base_framework.framework_output,
                         "ess:donau setting name"));

    if (NULL == prte_ess_base_nspace) {
        PRTE_ERROR_LOG(PRTE_ERR_NOT_FOUND);
        return PRTE_ERR_NOT_FOUND;
    }

    PMIX_LOAD_NSPACE(PRTE_PROC_MY_NAME->nspace, prte_ess_base_nspace);

    if (NULL == prte_ess_base_vpid) {
        PRTE_ERROR_LOG(PRTE_ERR_NOT_FOUND);
        return PRTE_ERR_NOT_FOUND;
    }
    vpid = strtoul(prte_ess_base_vpid, NULL, 10);

    donau_nodeid = atoi(getenv("CCS_NODE_RANK"));
    if (donau_nodeid < 0) {
        PRTE_ERROR_LOG(PRTE_ERR_INVALID_NODE_RANK);
        return PRTE_ERR_INVALID_NODE_RANK;
    }

    PMIX_OUTPUT_VERBOSE((1, prte_ess_base_framework.framework_output, 
                        "ess:donau set name to %s",
                        PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));
    PRTE_PROC_MY_NAME->rank = vpid + donau_nodeid - 1;

    PMIX_OUTPUT_VERBOSE((1, prte_ess_base_framework.framework_output,
                         "ess:donau set name to %s", PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));
    
    prte_process_info.num_daemons = prte_ess_base_num_procs;

    return PRTE_SUCCESS;
}
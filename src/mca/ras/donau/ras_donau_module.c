/*
 * Copyright (c) 2024      Huawei Technologies Co., Ltd.
 *                         All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "constants.h"
#include "prte_config.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include "src/util/pmix_argv.h"
#include "src/util/pmix_net.h"

#include "src/mca/errmgr/errmgr.h"
#include "src/mca/rmaps/base/base.h"
#include "src/mca/rmaps/rmaps_types.h"
#include "src/runtime/prte_globals.h"
#include "src/util/pmix_show_help.h"

#include "src/mca/ras/base/base.h"
#include "src/mca/ras/base/ras_private.h"
#include "ras_donau.h"

/*
 * Local functions
 */
static int prte_ras_donau_allocate(prte_job_t *jdata, pmix_list_t *nodes);
static int prte_ras_donau_finalize(void);
static int donau_get_alloc(char *alloc_path, pmix_list_t *nodes);

/*
 * RAS donau module
 */
prte_ras_base_module_t prte_ras_donau_module = {
    NULL,
    prte_ras_donau_allocate,
    NULL,
    prte_ras_donau_finalize
};

static int donau_get_alloc(char *alloc_path, pmix_list_t *nodes)
{
    int num_nodes = 0;
    prte_node_t *node = NULL;
    FILE *fp;
    fp = fopen(alloc_path, "r");
    if (NULL == fp) {
        return num_nodes;
    }
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, fp)) != -1) {
        char hostname[DONAU_MAX_NODENAME_LENGTH] = {0};
        int num_kernels = 0;
        int slots = 0;
        if (sscanf(line, "%s %d %d", hostname, &num_kernels, &slots) != 3) {
            pmix_output_verbose(10, prte_ras_base_framework.framework_output,
                                "ras/donau: Get the wrong num of params in CCS_ALLOC_FILE");
            break;
        }

        node = PMIX_NEW(prte_node_t);
        if (NULL == node) {
            num_nodes = 0;
            pmix_output_verbose(10, prte_ras_base_framework.framework_output,
                                "ras/donau: Failed when create obj of orte_node_t");
            goto cleanup;
        }
        node->name = strdup(hostname);
        // Strip off the FQDN if present, ignore IP addresses
        if (!prte_keep_fqdn_hostnames && !pmix_net_isaddr(node->name)) {
            char *ptr;
            if (NULL != (ptr = strchr(node->name, '.'))) {
                *ptr = '\0';
            }
        }
        node->state = PRTE_NODE_STATE_UP;
        node->slots_inuse = 0;
        node->slots_max = 0;
        node->slots = slots;
        pmix_list_append(nodes, &node->super);
        num_nodes++;
    }
    free(line);
    fclose(fp);
    return num_nodes;
cleanup:
    if (NULL != nodes) {
        PMIX_LIST_RELEASE(nodes);
    }
    free(line);
    fclose(fp);
    return num_nodes;
}

static int prte_ras_donau_allocate(prte_job_t *jdata, pmix_list_t *nodes) {
    int num_nodes;
    char *alloc_path = NULL;

    /* get the list of allocated nodes */
    alloc_path = getenv("CCS_ALLOC_FILE");
    if (NULL == alloc_path || 0 == strlen(alloc_path) ||
       ((num_nodes = donau_get_alloc(alloc_path, nodes))) <= 0) {
        pmix_show_help("help-ras-donau.txt", "nodelist-failed", true);
        return PRTE_ERR_NOT_AVAILABLE;
    }

    return PRTE_SUCCESS;
}

static int prte_ras_donau_finalize(void)
{
    return PRTE_SUCCESS;
}

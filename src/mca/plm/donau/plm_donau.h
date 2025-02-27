/*
 * Copyright (c) 2024      Huawei Technologies Co., Ltd.
 *                         All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PRTE_PLM_DONAU_EXPORT_H
#define PRTE_PLM_DONAU_EXPORT_H

#include "prte_config.h"

#include "src/mca/mca.h"
#include "src/mca/plm/plm.h"
#include "src/mca/oob/base/base.h"
#include "src/mca/plm/base/base.h"
#include "src/util/pmix_path.h"
#include "src/util/pmix_basename.h"
BEGIN_C_DECLS

struct prte_mca_plm_donau_component_t {
    prte_plm_base_component_t super;
    char *custom_args;
    bool donau_warning_msg;
};
typedef struct prte_mca_plm_donau_component_t prte_mca_plm_donau_component_t;

/*
 * Globally exported variable
 */

PRTE_MODULE_EXPORT extern prte_mca_plm_donau_component_t prte_mca_plm_donau_component;
PRTE_EXPORT extern prte_plm_base_module_t prte_plm_donau_module;

END_C_DECLS

#endif /* ORTE_PLM_DONAU_EXPORT_H */
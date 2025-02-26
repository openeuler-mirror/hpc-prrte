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
#include "src/mca/mca.h"
#include "src/mca/ess/ess.h"

#ifndef PRTE_ESS_DONAU_H
#define PRTE_ESS_DONAU_H

BEGIN_C_DECLS

PRTE_MODULE_EXPORT extern prte_ess_base_component_t prte_mca_ess_donau_component;

/*
 * Module open / close
 */
int prte_ess_donau_component_open(void);
int prte_ess_donau_component_close(void);
int prte_ess_donau_component_query(pmix_mca_base_module_t **module, int *priority);

END_C_DECLS

#endif /* ORTE_ESS_DONAU_H */
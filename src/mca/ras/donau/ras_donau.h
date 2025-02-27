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
/**
 * @file
 *
 * Resource Allocation(DONAU)
 */
#ifndef PRTE_RAS_DONAU_H
#define PRTE_RAS_DONAU_H

#include "prte_config.h"
#include "src/mca/ras/ras.h"
#include "src/mca/ras/base/base.h"

BEGIN_C_DECLS

/**
 * RAS Component
 */
typedef struct {
    prte_ras_base_component_t super;
    int param_priority;
} prte_mca_ras_donau_component_t;

PRTE_EXPORT extern prte_mca_ras_donau_component_t prte_mca_ras_donau_component;
PRTE_EXPORT extern prte_ras_base_module_t prte_ras_donau_module;

END_C_DECLS

#endif
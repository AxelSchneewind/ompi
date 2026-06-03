/*
 * Copyright (c) 2024-2026 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef AGGREGATION_SCHEMES_SELECT_FACTOR_H
#define AGGREGATION_SCHEMES_SELECT_FACTOR_H

#include "ompi_config.h"

/**
 * @brief selects an aggregation factor (i.e. number of user-partitions per transfer partition)
 * based on the given constraints on partition number and size.
 *
 * @param (IN)  parts                number of user-provided partitions
 * @param (IN)  count                size of user-provided partitions in elements
 * @param (IN)  max_parts            maximal number of internal partitions in elements
 * @param (IN)  min_count            minimal number of internal partitions
 * @param (OUT) factor               number of public partitions corresponding to each internal partitions other than the last one
 */
void aggregation_schemes_select_factor(size_t parts, size_t count, size_t max_parts, size_t min_count, size_t* factor);

#endif
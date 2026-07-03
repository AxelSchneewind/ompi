/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2024      High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

/**
 * @file
 * This file defines a simple message aggregation scheme:
 * A user-provided partitioning of n partitions of size b can be mapped
 * to an internal partitioning of n/k partitions of size k*b,
 * where k can be selected to optimize internal partition size.
 */

#ifndef AGGREGATION_SCHEME_IDENTITY_H
#define AGGREGATION_SCHEME_IDENTITY_H

#include "ompi_config.h"


/**
 * @brief tracks the number of pready calls corresponding to internal partitions
 *
 */
struct part_persist_identity_aggregation_state_t { };

/**
 * @brief initializes the aggregation state for the sending side
 *
 * @param[out] state                        pointer to aggregation state object
 * @param[in] internal_partition_count      number of internal partitions (i.e. number of messages
 * per partitioned transfer)
 * @param[in] factor                        number of public partitions corresponding to each internal one other than the last
 * @param[in] last_internal_partition_size  number of public partitions corresponding to last
 * internal partition
 */
void aggregation_scheme_identity_init(struct part_persist_identity_aggregation_state_t *state);

/**
 * @brief resets the aggregation state
 *
 * @param[out] state                pointer to aggregation state object
 */
void aggregation_scheme_identity_reset(struct part_persist_identity_aggregation_state_t *state);

/**
 * @brief marks a public partition as ready
 *
 * @param[in,out] state                pointer to aggregation state object
 * @param[in] part_min                 index of the first public partition to mark ready
 * @param[in] part_max                 index of the last  public partition to mark ready
 * @param[out] available_partition_min index of the first internal partition 
 * @param[out] available_partition_max index of the last internal partition 
 * @retval                             1 if partitions were extracted
 * @retval                             0 if nothing extracted
 */
int aggregation_scheme_identity_pready_range(struct part_persist_identity_aggregation_state_t *state,
                                             int part_min, int part_max, int* available_partition_min, int* available_partition_max);

/**
 * @brief destroys the aggregation scheme
 *
 * @param[in,out] state             pointer to aggregation state object
 */
void aggregation_scheme_identity_free(struct part_persist_identity_aggregation_state_t *state);

#endif

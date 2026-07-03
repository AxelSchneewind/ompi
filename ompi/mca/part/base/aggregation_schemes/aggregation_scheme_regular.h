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

#ifndef AGGREGATION_SCHEME_REGULAR_H
#define AGGREGATION_SCHEME_REGULAR_H

#include "ompi_config.h"

#include "opal/include/opal/sys/atomic.h"


/**
 * @brief tracks the number of pready calls corresponding to internal partitions
 *
 */
struct part_persist_regular_aggregation_state_t {
    // counters for each internal partition
    opal_atomic_int32_t *public_parts_ready;

    int public_partition_count;
    int internal_partition_count;

    // parameters for message aggregation
    int parts;
    int factor; // how many public partitions may be aggregated into an internal one

    int last_internal_partition_size; // number of public partitions corresponding to last internal one
};

/**
 * @brief selects an internal partitioning based on the user-provided partitioning
 * and the selected aggregation factor.
 *
 * More precisely, ensures that: (internal_partitions - 1) * factor + remainder == partitions
 * 
 * @param (IN)  partitions           number of user-provided partitions
 * @param (IN)  factor               number of public partitions corresponding to each internal partitions other than the last one
 * @param (OUT) internal_partitions  number of internal partitions
 * @param (OUT) remainder            number of public partitions corresponding to the last internal partition
 */
void aggregation_scheme_regular_select_internal_partitioning(size_t partitions, size_t factor, size_t* internal_partitions, size_t* remainder);

/**
 * @brief initializes the aggregation state for the sending side.
 * Ensures that exactly ceil(parts/factor) messages are transferred.
 *
 * @param[out] state          pointer to aggregation state object
 * @param[in] parts           number of public partitions 
 * @param[in] factor          number of public partitions corresponding to each internal one (other than the last internal partition)
 */
void aggregation_scheme_regular_init(struct part_persist_regular_aggregation_state_t *state,
                                    int parts, int factor);

/**
 * @brief resets the aggregation state
 *
 * @param[out] state                pointer to aggregation state object
 */
void aggregation_scheme_regular_reset(struct part_persist_regular_aggregation_state_t *state);

/**
 * @brief marks a public partition as ready
 *
 * @param[in,out] state                pointer to aggregation state object
 * @param[in] part_min                 index of the first public partition to mark ready
 * @param[in] part_max                 index of the last  public partition to mark ready
 * @param[out] available_partition_min index of the first public partition 
 * @param[out] available_partition_max index of the last  public partition 
 */
int aggregation_scheme_regular_pready_range(struct part_persist_regular_aggregation_state_t *state,
                                       int part_min, int part_max, int* available_partition_min, int* available_partition_max);

/**
 * @brief 
 *
 * @param[in,out] state             pointer to aggregation state object
 * @param[in] partition             index of the public partition
 * @return the internal partition number corresponding to the given 
 */
int aggregation_scheme_regular_internal_part(struct part_persist_regular_aggregation_state_t *state,
                                             int partition);

/**
 * @brief destroys the aggregation scheme
 *
 * @param[in,out] state             pointer to aggregation state object
 */
void aggregation_scheme_regular_free(struct part_persist_regular_aggregation_state_t *state);

#endif

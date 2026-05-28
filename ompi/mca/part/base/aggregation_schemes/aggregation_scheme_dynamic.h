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

#ifndef AGGREGATION_SCHEME_DYNAMIC_H
#define AGGREGATION_SCHEME_DYNAMIC_H

#include "ompi_config.h"

#include "opal/include/opal/sys/atomic.h"


enum aggregation_algorithm
{
    REGULAR,
    INTERVAL_TREE,
    RB_TREE
};

/**
 * @brief tracks the number of pready calls corresponding to internal partitions
 *
 */
struct part_persist_aggregation_state {
    aggregation_algorithm algorithm;
    void* aggregation_state;
};

/**
 * @brief initializes the aggregation state for the sending side
 *
 * @param[out] state                        pointer to aggregation state object
 * per partitioned transfer)
 * @param[in] algorithm                     algorithm to use
 * @param[in] factor                        number of public partitions corresponding to a transfer
 */
void aggregation_scheme_dynamic_psend_init(struct part_persist_aggregation_state *state, aggregation_algorithm algorithm, int factor)

/**
 * @brief resets the aggregation state
 *
 * @param[out] state                pointer to aggregation state object
 */
void aggregation_scheme_dynamic_reset(struct part_persist_aggregation_state *state);

/**
 * @brief marks a public partition as ready
 *
 * @param[in,out] state             pointer to aggregation state object
 * @param[in] partition             index of the public partition to mark ready
 * @param[out] available_partition  index of the internal partition if it is ready, otherwise -1
 */
void aggregation_scheme_dynamic_pready(struct part_persist_aggregation_state *state,
                                       int partition, int* available_partition_min, int* available_partition_last);

/**
 * @brief destroys the aggregation scheme
 *
 * @param[in,out] state             pointer to aggregation state object
 */
void aggregation_scheme_dynamic_free(struct part_persist_aggregation_state *state);

#endif

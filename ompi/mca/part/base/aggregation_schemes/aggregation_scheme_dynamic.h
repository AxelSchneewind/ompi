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


typedef enum 
{
    REGULAR,
    INTERVAL_TREE,
    RB_TREE
} aggregation_algorithm;

typedef void (*unary_fn_ptr)(void*); // function type
typedef void (*pready_fn_ptr)(void*, int, int*, int*); // function type
typedef void (*pready_range_fn_ptr)(void*, int, int, int*, int*); // function type

typedef void (*remaining_fn_ptr)(void*, void** remaining, size_t* remaining_count);

/**
 * @brief tracks the number of pready calls corresponding to internal partitions
 *
 */
struct part_persist_aggregation_state_t {
    aggregation_algorithm algorithm;
    void* aggregation_state;

    unary_fn_ptr reset;
    unary_fn_ptr free;
    pready_fn_ptr pready;
    pready_range_fn_ptr pready_range;
    remaining_fn_ptr remaining;
};

/**
 * @brief initializes the aggregation state for the sending side
 *
 * @param[out] state                        pointer to aggregation state object
 * per partitioned transfer)
 * @param[in] algorithm                     algorithm to use
 * @param[in] factor                        number of public partitions corresponding to a transfer
 */
void aggregation_scheme_dynamic_init(struct part_persist_aggregation_state_t *state, aggregation_algorithm algorithm, ...);

/**
 * @brief resets the aggregation state
 *
 * @param[out] state                pointer to aggregation state object
 */
void aggregation_scheme_dynamic_reset(struct part_persist_aggregation_state_t *state);

/**
 * @brief marks a public partition as ready
 *
 * @param[in,out] state             pointer to aggregation state object
 * @param[in] partition             index of the public partition to mark ready
 * @param[out] available_partition_min  index of the first internal partition if it is ready
 * @param[out] available_partition_max  index of the last internal partition if it is ready
 */
void aggregation_scheme_dynamic_pready(struct part_persist_aggregation_state_t *state,
                                       int partition, int* available_partition_min, int* available_partition_max);

/**
 * @brief marks public partitions as ready
 *
 * @param[in,out] state             pointer to aggregation state object
 * @param[in] partition_min             index of the first public partition to mark ready
 * @param[in] partition_min             index of the last public partition to mark ready
 * @param[out] available_partition_min  index of the first internal partition if it is ready
 * @param[out] available_partition_max  index of the last internal partition if it is ready
 */
void aggregation_scheme_dynamic_pready_range(struct part_persist_aggregation_state_t *state,
                                       int partition_min, int partition_max, int* available_partition_min, int* available_partition_max);

/**
 * @brief destroys the aggregation scheme
 *
 * @param[in,out] state             pointer to aggregation state object
 */
void aggregation_scheme_dynamic_free(struct part_persist_aggregation_state_t *state);

/**
 * @brief obtains all partitions that have not been consumed yet
 *
 * @param[in,out] state             pointer to aggregation state object
 * @param[out] remaining            pointer to the interval_state objects of remaining intervals
 * @param[out] remaining_count      number of remaining intervals
 */
void aggregation_scheme_dynamic_remaining(struct part_persist_aggregation_state_t *state, void** remaining, size_t *remaining_count);

#endif

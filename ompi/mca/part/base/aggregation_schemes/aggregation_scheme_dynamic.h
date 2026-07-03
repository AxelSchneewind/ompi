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
#include "opal/mca/threads/mutex.h"


typedef enum 
{
    IDENTITY = 0,
    REGULAR = 1,
    INTERVAL_TREE = 2,
    RB_TREE = 3
} aggregation_algorithm;

typedef void (*unary_fn_ptr)(void*); // function type
typedef int  (*pready_range_fn_ptr)(void*, int, int, int*, int*); // function type

typedef void (*remaining_fn_ptr)(void*, void** remaining, size_t* remaining_count);

/**
 * @brief tracks the number of pready calls corresponding to internal partitions
 *
 */
struct part_persist_aggregation_state_t {
    aggregation_algorithm algorithm;
    void* aggregation_state;

    // 
    opal_mutex_t lock;

    unary_fn_ptr reset;
    unary_fn_ptr free;
    pready_range_fn_ptr pready_range;
    remaining_fn_ptr remaining;
};

/**
 * @brief initializes the aggregation state for the sending side
 *
 * @param[out] state                        pointer to aggregation state object
 * per partitioned transfer)
 * @param[in] algorithm                     algorithm to use
 * @param[in] parts                         number of public partitions
 * @param[in] factor                        desired number of public partitions to be aggregated into a message.
 *                                          the selected algorithm is not required to guarantee that.
 */
void aggregation_scheme_dynamic_init(struct part_persist_aggregation_state_t *state, aggregation_algorithm algorithm, int parts, int factor);

/**
 * @brief resets the aggregation state
 *
 * @param[out] state                pointer to aggregation state object
 */
void aggregation_scheme_dynamic_reset(struct part_persist_aggregation_state_t *state);

/**
 * @brief marks a public partition as ready
 *
 * @param[in,out] state                 pointer to aggregation state object
 * @param[in] partition                 index of the public partition to mark ready
 * @param[out] available_partition_min  index of the first internal partition if it is ready
 * @param[out] available_partition_max  index of the last internal partition if it is ready, smaller than available_partition_min if none
 * @return                              1 if partitions were extracted, 0 if not
 */
int aggregation_scheme_dynamic_pready(struct part_persist_aggregation_state_t *state,
                                       int partition, int* available_partition_min, int* available_partition_max);

/**
 * @brief marks public partitions as ready
 *
 * @param[in,out] state                 pointer to aggregation state object
 * @param[in] part_min                  index of the first public partition to mark ready
 * @param[in] part_max                  index of the last public partition to mark ready
 * @param[out] available_partition_min  index of the first internal partition if it is ready
 * @param[out] available_partition_max  index of the last internal partition if it is ready, smaller than available_partition_min if none
 * @return                              1 if partitions were extracted, 0 if not
 */
int aggregation_scheme_dynamic_pready_range(struct part_persist_aggregation_state_t *state,
                                       int part_min, int part_max, int* available_partition_min, int* available_partition_max);

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

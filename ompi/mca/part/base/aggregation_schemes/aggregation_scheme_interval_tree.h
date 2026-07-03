/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2026      High Performance Computing Center Stuttgart,
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
 * This file defines an interval-tree based message aggregation scheme:
 * Intervals of marked partitions are tracked using an opal_interval_tree.
 * Whenever a new partition is marked ready, it gets added as an interval
 * or extends an existing one. If the updated interval is large enough, it gets
 * returned such that the interval can be transferred.
 */

#ifndef AGGREGATION_SCHEME_INTERVAL_TREE_H
#define AGGREGATION_SCHEME_INTERVAL_TREE_H

#include "ompi_config.h"

#include "aggregation_scheme.h"

#include "opal/include/opal/sys/atomic.h"
#include "opal/class/opal_interval_tree.h"


/**
 * @brief tracks the number of pready calls corresponding to internal partitions
 *
 */
struct part_persist_interval_tree_aggregation_state_t {
    // intervals of marked partitions
    opal_interval_tree_t intervals;

    // 
    // opal_list_t          interval_states;
    interval_state_t    *interval_states;
    opal_atomic_int64_t interval_count;

    // parameters for message aggregation
    int parts;
    int factor; // how many public partitions have to be aggregated into an internal one
};

/**
 * @brief initializes the aggregation state
 *
 * @param[out] state                        pointer to aggregation state object
 * @param[in] internal_partition_count      number of internal partitions (i.e. number of messages
 * per partitioned transfer)
 * @param[in] factor                        number of public partitions corresponding to each internal one other than the last
 * @param[in] last_internal_partition_size  number of public partitions corresponding to last
 * internal partition
 */
OMPI_DECLSPEC void aggregation_scheme_interval_tree_init(struct part_persist_interval_tree_aggregation_state_t *state, int parts, int factor);

/**
 * @brief resets the aggregation state
 *
 * @param[out] state                pointer to aggregation state object
 */
OMPI_DECLSPEC void
aggregation_scheme_interval_tree_reset(struct part_persist_interval_tree_aggregation_state_t *state);

/**
 * @brief marks a public partition as ready
 *
 * @param[in,out] state                  pointer to aggregation state object
 * @param[in] partition                  index of the public partition to mark ready
 * @param[out] available_partition_first left  index of an interval of available partitions (inclusive)
 * @param[out] available_partition_last  right index of an interval of available partitions (inclusive)
 * @returns 1 if there is an interval of available partitions, otherwise 0
 */
OMPI_DECLSPEC int aggregation_scheme_interval_tree_pready(struct part_persist_interval_tree_aggregation_state_t *state,
                                                              int partition, int* available_partitions_first, int* available_partitions_last);


/**
 * @brief marks a range of public partitions as ready
 *
 * @param[in,out] state                     pointer to aggregation state object
 * @param[in,out] min                       left  index of the interval to mark ready (inclusive)
 * @param[in,out] max                       right index of the interval to mark ready (inclusive)
 * @param[in,out] available_partition_first left  index of an interval of available partitions (inclusive)
 * @param[in,out] available_partition_last  right index of an interval of available partitions (inclusive)
 * @returns 1 if there is an interval of available partitions, otherwise 0
 */
OMPI_DECLSPEC int aggregation_scheme_interval_tree_pready_range(struct part_persist_interval_tree_aggregation_state_t *state,
                                                              int min, int max, 
                                                              int* available_partitions_first, int* available_partitions_last);
                                                    
/**
 * @brief obtains all partitions that have not been consumed yet
 *
 * @param[in,out] state             pointer to aggregation state object
 * @param[out] remaining            pointer to the interval_state objects of remaining intervals
 * @param[out] remaining_count      number of remaining intervals
 */
OPAL_DECLSPEC void aggregation_scheme_interval_tree_remaining(struct part_persist_interval_tree_aggregation_state_t *state, interval_state_t **remaining, size_t *remaining_count);

/**
 * @brief destroys the aggregation scheme
 *
 * @param[in,out] state             pointer to aggregation state object
 */
OMPI_DECLSPEC void aggregation_scheme_interval_tree_free(struct part_persist_interval_tree_aggregation_state_t *state);


#endif

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
 * This file defines an red-black-tree based message aggregation scheme:
 */

#ifndef aggregation_scheme_rb_tree_H
#define aggregation_scheme_rb_tree_H

#include "ompi_config.h"

#include "opal/include/opal/sys/atomic.h"
#include "opal/class/opal_rb_tree.h"


struct interval_state_t {
    int left;
    int right;
    int consumed;
};
typedef struct interval_state_t interval_state_t;


/**
 * @brief tracks the number of pready calls corresponding to internal partitions
 *
 */
struct part_persist_rb_tree_aggregation_state_t {
    // intervals of marked partitions
    opal_rb_tree_t intervals;

    // 
    // opal_list_t          interval_states;
    interval_state_t    *interval_states;
    opal_atomic_size_t  interval_count;

    // parameters for message aggregation
    int factor; // how many public partitions have to be aggregated into an internal one
};

/**
 * @brief initializes the aggregation state
 *
 * @param[out] state                        pointer to aggregation state object
 * @param[in] factor                        number of public partitions corresponding to each internal one other than the last
 */
OMPI_DECLSPEC void aggregation_scheme_rb_tree_init(struct part_persist_rb_tree_aggregation_state_t *state, int factor);

/**
 * @brief resets the aggregation state
 *
 * @param[out] state                pointer to aggregation state object
 */
OMPI_DECLSPEC void
aggregation_scheme_rb_tree_reset(struct part_persist_rb_tree_aggregation_state_t *state);

/**
 * @brief marks a public partition as ready
 *
 * @param[in,out] state                  pointer to aggregation state object
 * @param[in] partition                  index of the public partition to mark ready
 * @param[out] available_partition_first left  index of an interval of available partitions (inclusive)
 * @param[out] available_partition_last  right index of an interval of available partitions (inclusive)
 * @returns 1 if there is an interval of available partitions, otherwise 0
 */
OMPI_DECLSPEC int aggregation_scheme_rb_tree_pready(struct part_persist_rb_tree_aggregation_state_t *state,
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
OMPI_DECLSPEC int aggregation_scheme_rb_tree_pready_range(struct part_persist_rb_tree_aggregation_state_t *state,
                                                          int min, int max, 
                                                          int* available_partitions_first, int* available_partitions_last);
                                                    
/**
 * @brief obtains all partitions that have not been consumed yet
 *
 * @param[in,out] state             pointer to aggregation state object
 * @param[out] remaining            pointer to the interval_state objects of remaining intervals
 * @param[out] remaining_count      number of remaining intervals
 */
OPAL_DECLSPEC void aggregation_scheme_rb_tree_remaining(struct part_persist_rb_tree_aggregation_state_t *state, interval_state_t **remaining, size_t *remaining_count);

/**
 * @brief destroys the aggregation scheme
 *
 * @param[in,out] state             pointer to aggregation state object
 */
OMPI_DECLSPEC void aggregation_scheme_rb_tree_free(struct part_persist_rb_tree_aggregation_state_t *state);


#endif

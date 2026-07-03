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

#include "aggregation_scheme_rb_tree.h"

#include <stdlib.h>

// comparator for intervals: -1 if interval2 is left of interval1, 1 if interval2 right of interval1 and 0 if interval2 contained in interval1
// both ends are considered to be part of the interval
int interval_comp(interval_state_t* interval1, interval_state_t* interval2)
{
    // interval1 < interval2?
    if (interval2->left  > interval1->right) return 1;
    // interval2 < interval1?
    if (interval2->right < interval1->left) return -1;
    // overlap
    return 0;
}

opal_rb_tree_comp_fn_t interval_comp_fn = (opal_rb_tree_comp_fn_t) interval_comp;


void aggregation_scheme_rb_tree_init(struct part_persist_rb_tree_aggregation_state_t *state, int parts, int factor)
{
    // number of user-partitions per internal partition (except for the last one)
    state->parts = parts;
    state->factor = factor;

    state->interval_states = calloc(state->parts, sizeof(interval_state_t));
    state->interval_count = 0;

    // 
    OBJ_CONSTRUCT(&state->intervals, opal_rb_tree_t);
    opal_rb_tree_init(&state->intervals, interval_comp_fn);
}

void aggregation_scheme_rb_tree_reset(struct part_persist_rb_tree_aggregation_state_t *state)
{
    opal_atomic_swap_64(&state->interval_count, 0);

    opal_rb_tree_destroy(&state->intervals);
    opal_rb_tree_init(&state->intervals, interval_comp_fn);
}

// static opal_atomic_uint64_t call_counter;


void find_neighbors(struct part_persist_rb_tree_aggregation_state_t *state, int min, int max, interval_state_t** left, interval_state_t** right)
{

}

int aggregation_scheme_rb_tree_pready_range(struct part_persist_rb_tree_aggregation_state_t *state,
                                            int min, int max, 
                                            int* available_partitions_first, int* available_partitions_last)
{
    int err = OPAL_SUCCESS;
    // 
    interval_state_t sentinel;
    sentinel.left = -1; sentinel.right = -1;
    sentinel.consumed = true;

    // try to find adjacent intervals
    interval_state_t *left = &sentinel, *right = &sentinel; 
    interval_state_t lkey, rkey; 
    if (min > 0) 
    {
        lkey.left = min-1; lkey.right = min-1;
        left = opal_rb_tree_find(&state->intervals, &lkey);
    }
    if (max < state->parts-1)
    {
        rkey.left = max+1; rkey.right = max+1;
        right = opal_rb_tree_find(&state->intervals, &rkey);
    }

    bool empty_left  = NULL == left;
    bool empty_right = NULL == right;

    bool merge_left  = !empty_left  && !left->consumed;
    bool merge_right = !empty_right && !right->consumed;

    // to mark whether left/right is still the left/right neighbor of the interval after merging/inserting
    bool recheck_left = false;
    bool recheck_right = false;

    // int counter = opal_atomic_fetch_add_64(&call_counter, 1);
    // printf("%3i [%i,%i]: left empty/merge: %i,%i, right empty/merge: %i,%i\n", counter, min, max, empty_left, merge_left, empty_right, merge_right);
    // if (!empty_left)
    // printf("%3i\tleft  [%i,%i : %i]\n", counter, left->left,  left->right, left->consumed);
    // if (!empty_right)
    // printf("%3i\tright [%i,%i : %i]\n", counter, right->left, right->right, right->consumed);

    interval_state_t* candidate = NULL;

    if (merge_left && merge_right) // intervals can be merged
    {
        // mark right interval as consumed, as we might see it again when searching for remaining partitions
        right->consumed = true;
        err = opal_rb_tree_delete(&state->intervals, &rkey);
        assert(OPAL_SUCCESS == err);

        // extend the left interval
        left->right = right->right;

        candidate = left;

        // both neighbors of merged interval unknown
        recheck_left = true;
        recheck_right = true;

        // we can now assume that the candidate interval is blocked or free on both sides
    }
    else if (merge_left) // left interval can be extended
    {
        // extend interval
        left->right = max;

        candidate = left;

        // left neighbor of left unknown, right neighbor stays the same
        recheck_left = true;

        // we can now assume that the candidate interval is blocked or free on both sides
    }
    else if (merge_right) // right interval can be extended
    {
        // extend interval
        right->left = min;

        candidate = right;

        // right neighbor of right unknown, left neighbor stays the same
        recheck_right = true;

        // we can now assume that the candidate interval is blocked or free on both sides
    }
    else
    {
        // new interval state object
        size_t index = opal_atomic_fetch_add_64(&state->interval_count, 1);
        struct interval_state_t* new = &state->interval_states[index];
        new->left  = min;
        new->right = max;
        new->consumed = false;

        err = opal_rb_tree_insert(&state->intervals, new, new);
        assert(OPAL_SUCCESS == err);

        candidate = new;

        // both left and right neighbor are still correct

        // we can now assume that the candidate interval is blocked or free on both sides
    }

    bool do_extract = candidate->right + 1 - candidate->left >= state->factor;

    // if interval is too small, check if it is blocked
    if (!do_extract)
    {
        if (recheck_left) {
            left = &sentinel;
            if (candidate->left > 0)
            {
                lkey.left  = candidate->left - 1; lkey.right = candidate->left - 1;
                left = opal_rb_tree_find(&state->intervals, &lkey);
            }
        }
        if (recheck_right)
        {
            right = &sentinel;
            if (candidate->right < state->parts-1)
            {
                rkey.left  = candidate->right + 1; rkey.right = candidate->right + 1;
                right = opal_rb_tree_find(&state->intervals, &rkey);
            }
        }

        // if we found a neighbor, it has to be consumed already as otherwise, we would have merged earlier
        bool blocked_left  = NULL != left ;
        bool blocked_right = NULL != right;

        do_extract = blocked_left && blocked_right;
    }

    // printf("%3i\tcandidate [%i,%i]: extract: %i\n", counter, candidate->left, candidate->right, do_extract);

    if (do_extract)
    {
        candidate->consumed = true;
        *available_partitions_first = candidate->left;
        *available_partitions_last  = candidate->right;
        return 1;
    }
    else
    {
        *available_partitions_first = 0;
        *available_partitions_last  = -1;
        return 0;
    }
}

int aggregation_scheme_rb_tree_pready(struct part_persist_rb_tree_aggregation_state_t *state, int partition, int* available_partitions_left, int* available_partitions_right)
{
    return aggregation_scheme_rb_tree_pready_range(state, partition, partition, available_partitions_left, available_partitions_right);
}

// reuse interval_state list as list of remaining intervals
void aggregation_scheme_rb_tree_remaining(struct part_persist_rb_tree_aggregation_state_t *state, interval_state_t** remaining, size_t* remaining_count)
{
    opal_rb_tree_destroy(&state->intervals);

    int count = opal_atomic_swap_64(&state->interval_count, 0);
    for (int i = 0; i < count; i++)
    {
        if (state->interval_states[i].consumed)
        {
            --count;
            // take from end
            state->interval_states[i] = state->interval_states[count];
        }
    }

    *remaining = state->interval_states;
    *remaining_count = count;
}

void aggregation_scheme_rb_tree_free(struct part_persist_rb_tree_aggregation_state_t *state)
{
    if (state->interval_states != NULL)
        free((void*)state->interval_states);
    state->interval_states = NULL;

    opal_rb_tree_destroy(&state->intervals);
    OBJ_DESTRUCT(&state->intervals);
}


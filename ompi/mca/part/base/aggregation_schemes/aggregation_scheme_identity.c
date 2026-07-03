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

#include "aggregation_scheme_identity.h"

void aggregation_scheme_identity_init(struct part_persist_identity_aggregation_state_t *state)
{ }

void aggregation_scheme_identity_reset(struct part_persist_identity_aggregation_state_t *state)
{ }

int aggregation_scheme_identity_pready_range(struct part_persist_identity_aggregation_state_t *state,
                                       int part_min, int part_max, int* available_partition_min, int* available_partition_max)
{
    *available_partition_min = part_min;
    *available_partition_max = part_max;
    return 1;
}

void aggregation_scheme_identity_free(struct part_persist_identity_aggregation_state_t *state)
{
}

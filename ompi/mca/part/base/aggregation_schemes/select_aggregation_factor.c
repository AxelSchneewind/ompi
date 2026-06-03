/*
 * Copyright (c) 2024-2026 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include "ompi/mca/part/base/aggregation_schemes/select_aggregation_factor.h"

void aggregation_schemes_select_factor(size_t parts, size_t count, size_t max_parts, size_t min_count, size_t* factor) {
    size_t buffer_size = parts * count;

    // check if max_parts imposes higher limit on partition size
    if (max_parts > 0 && (buffer_size / max_parts) > min_count) {
        min_count = buffer_size / max_parts;
    }

    // cannot have parts larger than buffer size
    if (min_count > buffer_size) {
        min_count = buffer_size;
    }

    size_t _factor;

    if (count < min_count) {    // have to use larger partititions
        _factor = min_count / count;
    } else {    // can keep original partitioning
        _factor = 1;
    }

    *factor = _factor;
}

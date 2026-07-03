#ifndef AGGREGATION_SCHEME_H
#define AGGREGATION_SCHEME_H

#include "ompi_config.h"


struct interval_state_t {
    int left;
    int right;
    int consumed;
};
typedef struct interval_state_t interval_state_t;


#endif

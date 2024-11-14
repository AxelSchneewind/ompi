
/**
 * @file generic interface for aggregation schemes:
 * An aggregation scheme provides a queue-like interface that allows for 
 * insertion of user-partitions and extraction of available internal 
 * (transfer-) partitions. 
 * Such a scheme may decide the order in which transfer-partitions are
 * reported as ready. 
 */


typedef void* state_ptr;

// function pointer types
typedef void (*part_persist_aggregate_init_t)(state_ptr *state, int internal_partition_count, int public_partition_count);
typedef void (*part_persist_aggregate_free_t)(state_ptr *state);
typedef void (*part_persist_aggregate_push_t)(state_ptr state, int partition);
typedef int  (*part_persist_aggregate_pull_t)(state_ptr state);

/**
 * @brief struct to define an aggregation scheme
 * 
 */
struct aggregation_scheme {
    part_persist_aggregate_init_t init;
    part_persist_aggregate_free_t free;
    part_persist_aggregate_push_t push;
    part_persist_aggregate_pull_t pull;
};

/*
 * Copyright (c) 2004-2006 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2004-2007 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2015      Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * Copyright (c) 2017      Intel, Inc. All rights reserved
 * Copyright (c) 2019-2021 The University of Tennessee at Chattanooga and The University
 *                         of Tennessee Research Foundation. All rights reserved.
 * Copyright (c) 2019-2021 Sandia National Laboratories. All rights reserved.
 * Copyright (c) 2021      University of Alabama at Birmingham. All rights reserved.
 * Copyright (c) 2021      Tennessee Technological University. All rights reserved.
 * Copyright (c) 2021      Cisco Systems, Inc.  All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PART_DIRECT_H
#define PART_DIRECT_H

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <math.h>

#include "ompi_config.h"
#include "ompi/request/request.h"
#include "ompi/mca/part/part.h"
#include "ompi/mca/part/base/base.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/communicator/communicator.h"
#include "ompi/request/request.h"
#include "opal/sys/atomic.h"
#include "opal/class/opal_ring_buffer.h"

#include "ompi/mca/part/direct/part_direct_request.h"
#include "ompi/mca/part/base/part_base_precvreq.h"
#include "ompi/mca/part/direct/part_direct_recvreq.h"
#include "ompi/mca/part/direct/part_direct_sendreq.h"
#include "ompi/message/message.h"
#include "ompi/mca/pml/pml.h"

#include "ompi/mca/part/base/aggregation_schemes/aggregation_scheme_interval_tree.h"

#include "ompi/mca/part/base/aggregation_schemes/select_aggregation_factor.h"

BEGIN_C_DECLS

typedef struct mca_part_direct_list_t {
    opal_list_item_t        super;
    mca_part_direct_request_t *item;
} mca_part_direct_list_t;

OPAL_DECLSPEC OBJ_CLASS_DECLARATION(mca_part_direct_list_t);


struct ompi_part_direct_t {
    mca_part_base_module_t super;
    int                    free_list_num;
    int                    free_list_max;
    int                    free_list_inc;
    int                    min_message_size;
    int                    max_message_count;
    int                    algorithm;
    opal_list_t           *progress_list;

    opal_atomic_int32_t    block_entry;
    opal_mutex_t lock; 
};
typedef struct ompi_part_direct_t ompi_part_direct_t;
extern ompi_part_direct_t ompi_part_direct;


// type that can be interpreted as void* and therefore be inserted into opal datastructures
// guarantees that any non-empty interval as void* is distinct from NULL
struct partition_interval_queue_element 
{ 
    union { 
        struct { int begin; int len; } __attribute__((__packed__)) __attribute__((__aligned__));
        struct { void* as_ptr; } __attribute__((__packed__)) __attribute__((__aligned__)); 
    };
};

/**
 * This is a helper function that frees a request. This requires ompi_part_direct.lock be held before calling.
 */
__opal_attribute_always_inline__ static inline int
mca_part_direct_free_req(struct mca_part_direct_request_t* req)
{
    int err = OMPI_SUCCESS;
    opal_list_remove_item(ompi_part_direct.progress_list, (opal_list_item_t*)req->progress_elem);
    OBJ_RELEASE(req->progress_elem);

    MPI_Win_free(&req->window);
    MPI_Win_free(&req->window_flags);
    MPI_Comm_free(&req->comm);
    free(req->flags);

    if( MCA_PART_DIRECT_REQUEST_PRECV == req->req_type ) {
        MCA_PART_DIRECT_PRECV_REQUEST_RETURN(req);
    } else {
        MCA_PART_DIRECT_PSEND_REQUEST_RETURN(req);
    }
    return err;
}


__opal_attribute_always_inline__ static inline void mca_part_direct_init_lists(void)
{
    opal_free_list_init (&mca_part_direct_precv_requests,
                         sizeof(mca_part_direct_precv_request_t),
                         opal_cache_line_size,
                         OBJ_CLASS(mca_part_direct_precv_request_t),
                         0,opal_cache_line_size,
                         ompi_part_direct.free_list_num,
                         ompi_part_direct.free_list_max,
                         ompi_part_direct.free_list_inc,
                         NULL, 0, NULL, NULL, NULL);
    opal_free_list_init (&mca_part_direct_psend_requests,
                         sizeof(mca_part_direct_psend_request_t),
                         opal_cache_line_size,
                         OBJ_CLASS(mca_part_direct_psend_request_t),
                         0,opal_cache_line_size,
                         ompi_part_direct.free_list_num,
                         ompi_part_direct.free_list_max,
                         ompi_part_direct.free_list_inc,
                         NULL, 0, NULL, NULL, NULL);
     ompi_part_direct.progress_list = OBJ_NEW(opal_list_t);
}

__opal_attribute_always_inline__ static inline void
mca_part_direct_complete(struct mca_part_direct_request_t* request)
{
    if(MCA_PART_DIRECT_REQUEST_PRECV == request->req_type) {
        request->req_ompi.req_status.MPI_SOURCE = request->req_peer; 
    } else {
        request->req_ompi.req_status.MPI_SOURCE = request->req_comm->c_my_rank;
    }
    request->req_ompi.req_complete_cb = NULL;
    request->req_ompi.req_status.MPI_TAG = request->req_tag;  
    request->req_ompi.req_status._ucount = request->req_bytes;
    request->req_ompi.req_status.MPI_ERROR = OMPI_SUCCESS;
    request->req_part_complete = true;
    ompi_request_complete(&(request->req_ompi), true );
}

/**
 * mca_part_direct_progress is the progress function that will be registered. It handles 
 * both send and recv request testing and completion. It also handles freeing requests,
 * after MPI_Free is called and the requests have become inactive.
 */
__opal_attribute_always_inline__ static inline int
mca_part_direct_progress(void)
{
    mca_part_direct_list_t *current;
    int err;

    /* prevent re-entry, */
    int block_entry = opal_atomic_add_fetch_32(&(ompi_part_direct.block_entry), 1);
    if(1 < block_entry)
    {
        block_entry = opal_atomic_add_fetch_32(&(ompi_part_direct.block_entry), -1);
        return OMPI_SUCCESS;
    }

    OPAL_THREAD_LOCK(&ompi_part_direct.lock);
 
    mca_part_direct_request_t* to_delete = NULL;

    OPAL_LIST_FOREACH(current, ompi_part_direct.progress_list, mca_part_direct_list_t) {
        mca_part_direct_request_t *req = (mca_part_direct_request_t *) current->item;
        if(MCA_PART_DIRECT_REQUEST_PSEND == req->req_type)
        {
            if(false == req->req_part_complete && REQUEST_COMPLETED != req->req_ompi.req_complete && OMPI_REQUEST_ACTIVE == req->req_ompi.req_state && req->round != req->tround) {
                mca_part_direct_psend_request_t *sendreq = (mca_part_direct_psend_request_t *) req;

                size_t done_count = opal_atomic_fetch_add_64(&req->done_count, 0);
                size_t mark_count = opal_atomic_fetch_add_64(&req->mark_count, 0);

                /* Iterate through partition intervals that are queued for being started. Only applicable to sends. */ 
                while(done_count < mark_count)
                {
                    struct partition_interval_queue_element interval;
                    interval.as_ptr = opal_ring_buffer_pop(&sendreq->available_intervals);
                    if (NULL == interval.as_ptr) break;

                    err = MPI_Put(req->buf + interval.begin * req->part_bytes, interval.len * req->part_bytes, 
                                  MPI_CHAR, 1,
                                  interval.begin * req->part_bytes, interval.len * req->part_bytes, 
                                  MPI_CHAR, req->window);
                    assert(MPI_SUCCESS == err);

                    done_count = opal_atomic_add_fetch_64(&req->done_count, interval.len);
                    mark_count = opal_atomic_fetch_add_64(&req->mark_count, 0);
                    opal_output_verbose(6, ompi_part_base_framework.framework_output, "called put on [%i,%i]\n", interval.begin, interval.begin + interval.len - 1);
                }

                if (mark_count > req->parts)
                    opal_output_verbose(0, ompi_part_base_framework.framework_output, "marked %lu intervals of %lu, this should not happen\n", mark_count, req->parts);
                if (done_count > req->parts)
                    opal_output_verbose(0, ompi_part_base_framework.framework_output, "sent   %lu intervals of %lu, this should not happen\n", req->done_count, req->parts);

                // if mark_count reaches req->parts, collect remaining partitions
                if (done_count < req->parts && mark_count >= req->parts)
                {
                    // put remaining
                    interval_state_t* remaining;
                    size_t remaining_count;
                    aggregation_scheme_dynamic_remaining(&sendreq->aggregation_state, (void**)&remaining, &remaining_count);

                    for (size_t r = 0; r < remaining_count; r++)
                    {
                        interval_state_t interval = remaining[r];
                        size_t count = (interval.right - interval.left + 1);
                        err = MPI_Put(req->buf + interval.left * req->part_bytes, count * req->part_bytes, 
                                      MPI_CHAR, 1,
                                      interval.left * req->part_bytes, count * req->part_bytes, 
                                      MPI_CHAR, req->window);
                        assert(MPI_SUCCESS == err);

                        opal_output_verbose(6, ompi_part_base_framework.framework_output, "called put on [%i,%i]\n", interval.left, interval.right);

                        done_count = opal_atomic_add_fetch_64(&req->done_count, count);
                    }
                    opal_output_verbose(6, ompi_part_base_framework.framework_output, "collected %lu remaining intervals (marked: %lu, done: %lu)\n", remaining_count, mark_count, done_count);
                    assert(done_count == req->parts);
                }

                /* Check for completion and complete the requests */
                if(done_count >= req->parts)
                {
	                /* Increment round on reciever */
                    req->round++;
                    MPI_Win_flush(1,req->window);
                    MPI_Put(&req->round, 1, MPI_INT, 1, 0, 1, MPI_INT, req->window_flags);
                    MPI_Win_flush(1,req->window_flags);

                    mca_part_direct_complete(req);
                }
            }	    
        } else {
            if(false == req->req_part_complete && REQUEST_COMPLETED != req->req_ompi.req_complete && OMPI_REQUEST_ACTIVE == req->req_ompi.req_state) {
		        if(req->round == req->tround) {
                    mca_part_direct_complete(req);
		        }
	        }
	    }

        if(true == req->req_free_called && true == req->req_part_complete && REQUEST_COMPLETED == req->req_ompi.req_complete &&  OMPI_REQUEST_INACTIVE == req->req_ompi.req_state) {
            to_delete = req;
        }
    }
    OPAL_THREAD_UNLOCK(&ompi_part_direct.lock);
    block_entry = opal_atomic_add_fetch_32(&(ompi_part_direct.block_entry), -1);
    if(to_delete) {
        err =  mca_part_direct_free_req(to_delete);
        if (OMPI_SUCCESS != err) {
            return OMPI_ERROR;
        }
    }

    return OMPI_SUCCESS;
}

__opal_attribute_always_inline__ static inline void
mca_part_direct_create_partition_communicator(MPI_Comm comm,
                                   int rank_count,
                                   const int ranks[],
                                   MPI_Comm* new_comm)
{
    int err = MPI_SUCCESS;
    MPI_Group group_super, group_sub;

    err = ompi_comm_group(comm, &group_super);
    assert(MPI_SUCCESS == err);

    err = ompi_group_incl(group_super, rank_count, ranks, &group_sub);
    assert(MPI_SUCCESS == err);

    err = ompi_comm_create_group(comm, group_sub, 0, new_comm);
    assert(MPI_SUCCESS == err);
}


__opal_attribute_always_inline__ static inline int
mca_part_direct_precv_init(void *buf,
                        size_t parts, 
                        size_t count,
                        ompi_datatype_t * datatype,
                        int src,
                        int tag,
                        struct ompi_communicator_t *comm,
			struct ompi_info_t * info,
                        struct ompi_request_t **request)
{
    int err = OMPI_SUCCESS;
    size_t dt_size_;
    int dt_size;
    mca_part_direct_list_t* new_progress_elem = NULL;

    mca_part_direct_precv_request_t *recvreq;


    /* Allocate a new request */
    MCA_PART_DIRECT_PRECV_REQUEST_ALLOC(recvreq);
    if (OPAL_UNLIKELY(NULL == recvreq)) return OMPI_ERR_OUT_OF_RESOURCE;

    MCA_PART_DIRECT_PRECV_REQUEST_INIT(recvreq, ompi_proc, comm, tag, src,
                                     datatype, buf, parts, count, flags);

    mca_part_direct_request_t *req = (mca_part_direct_request_t *) recvreq;

    /* Set lazy initializion flags */
    req->flags = NULL;
    /* Non-blocking recive on setup info */

    /* Compute total number of bytes */
    err = opal_datatype_type_size(&(req->req_datatype->super), &dt_size_);
    if(OMPI_SUCCESS != err) return OMPI_ERROR;
    dt_size = (dt_size_ > (size_t) INT_MAX) ? MPI_UNDEFINED : (int) dt_size_;
    req->req_bytes = parts * count * dt_size;


    req->round = 0;
    req->tround = 0;

    int rank_super;
    err = MPI_Comm_rank(comm, &rank_super);
    int rank_count = 2;
    int ranks[rank_count];
    ranks[0] = src;
    ranks[1] = rank_super;
    mca_part_direct_create_partition_communicator(comm, rank_count, ranks, &req->comm);

    err = MPI_Win_create(buf,
                         parts * count * dt_size,
                         1,
                         MPI_INFO_NULL,
                         req->comm,
                         &req->window);
    assert(MPI_SUCCESS == err);

    err = MPI_Win_lock_all(1, req->window); fflush(stdout);
    assert(MPI_SUCCESS == err);

    err = MPI_Win_create(&req->round,
                         sizeof(int32_t),
                         sizeof(int32_t),
                         MPI_INFO_NULL,
                         req->comm,
                         &req->window_flags);
    assert(MPI_SUCCESS == err);

    err = MPI_Win_lock_all(1, req->window_flags);
    assert(MPI_SUCCESS == err);


    /* Set ompi request initial values */
    req->req_ompi.req_persistent = true;
    req->req_part_complete = true;
    req->req_ompi.req_type = OMPI_REQUEST_PART;
    req->req_ompi.req_complete = REQUEST_COMPLETED;
    req->req_ompi.req_state = OMPI_REQUEST_INACTIVE;

    /* Add element to progress engine */
    new_progress_elem = OBJ_NEW(mca_part_direct_list_t);
    new_progress_elem->item = req;
    req->progress_elem = new_progress_elem; 
    OPAL_THREAD_LOCK(&ompi_part_direct.lock);
    opal_list_append(ompi_part_direct.progress_list, (opal_list_item_t*)new_progress_elem);
    OPAL_THREAD_UNLOCK(&ompi_part_direct.lock);

    /* set return values */
    *request = (ompi_request_t*) recvreq;
    return err;
}


__opal_attribute_always_inline__ static inline int
mca_part_direct_psend_init(const void* buf,
                        size_t parts,
                        size_t count,
                        ompi_datatype_t* datatype,
                        int dst,
                        int tag,
                        ompi_communicator_t* comm,
			struct ompi_info_t * info,
                        ompi_request_t** request)
{
    int err = OMPI_SUCCESS;
    size_t dt_size_;
    int dt_size;
    mca_part_direct_list_t* new_progress_elem = NULL;
    mca_part_direct_psend_request_t *sendreq;

    /* Create new request object */
    MCA_PART_DIRECT_PSEND_REQUEST_ALLOC(sendreq, comm, dst, ompi_proc);
    if (OPAL_UNLIKELY(NULL == sendreq)) return OMPI_ERR_OUT_OF_RESOURCE;
    MCA_PART_DIRECT_PSEND_REQUEST_INIT(sendreq, ompi_proc, comm, tag, dst,
                                    datatype, buf, parts, count, flags);
    mca_part_direct_request_t *req = (mca_part_direct_request_t *) sendreq;


    /* Determine total bytes to send. */
    err = opal_datatype_type_size(&(req->req_datatype->super), &dt_size_);
    if(OMPI_SUCCESS != err) return OMPI_ERROR;
    dt_size = (dt_size_ > (size_t) INT_MAX) ? MPI_UNDEFINED : (int) dt_size_;
    req->req_bytes = parts * count * dt_size;
    req->part_bytes = count * dt_size;
    req->datatype = datatype;

    req->parts = parts;
    req->count = count;
    req->buf = (uint8_t*)buf;

    req->flags = (int*) calloc(req->parts, sizeof(int));

    req->round = 0;
    req->tround = 0;

    /* init aggregation state */
    size_t factor;
    aggregation_schemes_select_factor(parts, count, ompi_part_direct.max_message_count, ompi_part_direct.min_message_size / dt_size, &factor);

    aggregation_algorithm algorithm = (aggregation_algorithm) ompi_part_direct.algorithm;
    aggregation_scheme_dynamic_init(&sendreq->aggregation_state, algorithm, parts, factor);
    opal_output_verbose(5, ompi_part_base_framework.framework_output, "aggregating %lu*%lu partitioning with factor %lu and algorithm %i\n", parts, count, factor, algorithm);

    /* init partition interval queue */
    opal_ring_buffer_init(&sendreq->available_intervals, req->parts);

    int rank_super;
    err = MPI_Comm_rank(comm, &rank_super);
    int rank_count = 2;
    int ranks[rank_count];
    ranks[0] = rank_super;
    ranks[1] = dst;
    mca_part_direct_create_partition_communicator(comm, rank_count, ranks, &req->comm);

    err = MPI_Win_create(req->buf,
                         0,
                         // req->req_bytes,
                         1,
                         MPI_INFO_NULL,
                         req->comm,
                         &req->window);
    assert(MPI_SUCCESS == err);

    err = MPI_Win_lock_all(1, req->window); fflush(stdout);
    assert(MPI_SUCCESS == err);

    err = MPI_Win_create(&req->round,
                         sizeof(int32_t),
                         sizeof(int32_t),
                         MPI_INFO_NULL,
                         req->comm,
                         &req->window_flags);
    assert(MPI_SUCCESS == err);

    err = MPI_Win_lock_all(1, req->window_flags);
    assert(MPI_SUCCESS == err);

    /* Initilaize completion variables */
    sendreq->req_base.req_ompi.req_persistent = true;
    req->req_part_complete = true;
    req->req_ompi.req_type = OMPI_REQUEST_PART;
    req->req_ompi.req_complete = REQUEST_COMPLETED;
    req->req_ompi.req_state = OMPI_REQUEST_INACTIVE;
 
    /* add element to progress queue */
    new_progress_elem = OBJ_NEW(mca_part_direct_list_t);
    new_progress_elem->item = req;
    req->progress_elem = new_progress_elem;
    OPAL_THREAD_LOCK(&ompi_part_direct.lock);
    opal_list_append(ompi_part_direct.progress_list, (opal_list_item_t*)new_progress_elem);
    OPAL_THREAD_UNLOCK(&ompi_part_direct.lock);

    /* Set return values */
    *request = (ompi_request_t*) sendreq;
    return err;
}

__opal_attribute_always_inline__ static inline int
mca_part_direct_start(size_t count, ompi_request_t** requests)
{
    int err = OMPI_SUCCESS;
    size_t _count = count;
    size_t i;

    for(i = 0; i < _count && OMPI_SUCCESS == err; i++) {
        mca_part_direct_request_t *req = (mca_part_direct_request_t *)(requests[i]);
        req->tround++;
        if(MCA_PART_DIRECT_REQUEST_PSEND == req->req_type) {
            req->done_count = 0;
            opal_atomic_swap_64(&req->mark_count, 0);
            // req->available_count = 0;

            mca_part_direct_psend_request_t *sendreq = (mca_part_direct_psend_request_t *) req;
            aggregation_scheme_dynamic_reset(&sendreq->aggregation_state);
        } else {
            req->done_count = 0;
            opal_atomic_swap_64(&req->mark_count, 0);
	        // /* Increment round on sender */
	        // MPI_Put(&req->round, 1, MPI_INT, 0, 0, 1, MPI_INT, req->window_flags);
	        // MPI_Win_flush(0,req->window_flags);
        } 
        req->req_ompi.req_state = OMPI_REQUEST_ACTIVE;    
        req->req_ompi.req_status.MPI_TAG = MPI_ANY_TAG;
        req->req_ompi.req_status.MPI_ERROR = OMPI_SUCCESS;
        req->req_ompi.req_status._cancelled = 0;
        req->req_part_complete = false;
        req->req_ompi.req_complete = false;
        OPAL_ATOMIC_SWAP_PTR(&req->req_ompi.req_complete, REQUEST_PENDING);   
    }

    return err;
}

__opal_attribute_always_inline__ static inline int
mca_part_direct_pready(size_t min_part,
                    size_t max_part,
                    ompi_request_t* request)
{
    int err = OMPI_SUCCESS;

    mca_part_direct_request_t *req = (mca_part_direct_request_t *)(request);
    mca_part_direct_psend_request_t *sendreq = (mca_part_direct_psend_request_t *) req;

    int left, right;
    int extracted = aggregation_scheme_dynamic_pready_range(&sendreq->aggregation_state, min_part, max_part, &left, &right);

    if (extracted)
    {   // interval ready to transfer
        struct partition_interval_queue_element interval = { .begin = left, .len = right - left + 1 };

        err = MPI_Put(req->buf + interval.begin * req->part_bytes, interval.len * req->part_bytes, 
                      MPI_CHAR, 1,
                      interval.begin * req->part_bytes, interval.len * req->part_bytes, 
                      MPI_CHAR, req->window);
        assert(MPI_SUCCESS == err);

        opal_output_verbose(6, ompi_part_base_framework.framework_output, "called put on [%i,%i]\n", interval.begin, interval.begin + interval.len - 1);

        opal_atomic_fetch_add_64(&req->done_count, interval.len);
    }

    // has to happen after updating datastructures
    opal_atomic_fetch_add_64(&req->mark_count, max_part - min_part + 1); 

    return err;
}

__opal_attribute_always_inline__ static inline int
mca_part_direct_parrived(size_t min_part,
                      size_t max_part,
                      int* flag, 
                      ompi_request_t* request)
{
    int err = OMPI_SUCCESS;
    mca_part_direct_request_t *req = (mca_part_direct_request_t *)request;

    *flag = (req->round == req->tround); /* Rationale: RMA is performant implementation for n->1 partitions, and this is an opt-in performance version, we implement all partitioned communications as n->1 for this module. */
    return err;
}


/**
 * mca_part_direct_free marks an entry as free called and sets the request to 
 * MPI_REQUEST_NULL. Note: requests get freed in the progress engine. 
 */
__opal_attribute_always_inline__ static inline int
mca_part_direct_free(ompi_request_t** request)
{
    mca_part_direct_request_t* req = *(mca_part_direct_request_t**)request;

    if(true == req->req_free_called) return OMPI_ERROR;
    req->req_free_called = true;

    *request = MPI_REQUEST_NULL;
    return OMPI_SUCCESS;
}

END_C_DECLS

#endif  /* PART_DIRECT_H_HAS_BEEN_INCLUDED */

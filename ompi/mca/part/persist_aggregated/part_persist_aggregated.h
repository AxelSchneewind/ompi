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
 * Copyright (c) 2021      Bull S.A.S. All rights reserved.
 * Copyright (c) 2024      High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PART_persist_aggregated_H
#define PART_persist_aggregated_H

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include<math.h>

#include "ompi_config.h"
#include "ompi/request/request.h"
#include "ompi/mca/part/part.h"
#include "ompi/mca/part/base/base.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/communicator/communicator.h"
#include "ompi/request/request.h"
#include "opal/sys/atomic.h"

#include "ompi/mca/part/persist_aggregated/part_persist_aggregated_request.h"
#include "ompi/mca/part/base/part_base_precvreq.h"
#include "ompi/mca/part/persist_aggregated/part_persist_aggregated_recvreq.h"
#include "ompi/mca/part/persist_aggregated/part_persist_aggregated_sendreq.h"
#include "ompi/message/message.h"
#include "ompi/mca/pml/pml.h"
BEGIN_C_DECLS

typedef struct mca_part_persist_aggregated_list_t {
    opal_list_item_t        super;
    mca_part_persist_aggregated_request_t *item;
} mca_part_persist_aggregated_list_t;

OPAL_DECLSPEC OBJ_CLASS_DECLARATION(mca_part_persist_aggregated_list_t);


struct ompi_part_persist_aggregated_t {
    mca_part_base_module_t super;
    int                    free_list_num;
    int                    free_list_max;
    int                    free_list_inc;
    opal_list_t           *progress_list;

    int32_t next_send_tag;                /**< This is a counter for send tags for the actual data transfer. */
    int32_t next_recv_tag; 
    ompi_communicator_t   *part_comm; /* This approach requires a separate tag space, so we need a dedicated communicator. */
    ompi_request_t        *part_comm_req;
    int32_t                part_comm_ready;
    ompi_communicator_t   *part_comm_setup; /* We create a second communicator to send set-up messages (rational: these 
                                               messages go in the opposite direction of normal messages, need to use MPI_ANY_SOURCE 
                                               to support different communicators, and thus need to have a unique tag. Because tags 
                                               are controlled by the sender in this model, we cannot assume that the tag will be 
                                               unused in part_comm. */
    ompi_request_t        *part_comm_sreq;
    int32_t                part_comm_sready;
    int32_t                init_comms;    
    int32_t                init_world;
    int32_t                my_world_rank; /* Because the back end communicators use a world rank, we need to communicate ours 
                                             to set up the requests. */

    uint32_t min_message_size;          /* parameters to control internal partitioning */
    uint32_t max_message_count;

    opal_atomic_int32_t    block_entry;
    opal_mutex_t lock; 
};
typedef struct ompi_part_persist_aggregated_t ompi_part_persist_aggregated_t;
extern ompi_part_persist_aggregated_t ompi_part_persist_aggregated;



/**
 * @brief selects an internal partitioning based on the user-provided partitioning
 * and the mca parameters for minimal partition size and maximal partition count.
 * 
 * More precisely, given a partitioning into p partitions of size s, computes
 * an internal partitioning into p' partitions of size s' (apart from the last one,
 * which has potentially different size r * s):
 *      p * s = (p' - 1) * s' + r * s
 * where
 *      s' >= s
 *      p' <= p
 *      0 < r * s <= s'
 * and
 *      s' <= max_message_count
 *      p' >= min_message_size
 * (given by mca parameters).
 * 
 * @param[in]  partitions           number of user-provided partitions
 * @param[in]  count                size of user-provided partitions in elements
 * @param[out] internal_partitions  number of internal partitions
 * @param[out] factor               number of public partitions corresponding to each internal partitions other than the last one
 * @param[out] last_size            number of public partitions corresponding to the last internal partition
 */
static inline void part_persist_aggregated_select_internal_partitioning(size_t partitions, size_t part_size, size_t* internal_partitions, size_t* factor, size_t* remainder) {
    size_t buffer_size = partitions * part_size;
    int min_part_size  = ompi_part_persist_aggregated.min_message_size;
    int max_part_count = ompi_part_persist_aggregated.max_message_count;

    // check if max_part_count imposes higher limit on partition size
    if ((buffer_size / max_part_count) > min_part_size) {
        min_part_size = buffer_size / max_part_count;
    }

    // check if min_part_size imposes lower limit on partition count
    if ((buffer_size / min_part_size) < max_part_count) {
        max_part_count = buffer_size / min_part_size;
    }

    if (part_size < min_part_size || partitions > max_part_count) {    // have to use larger partititions
        // solve p = (p' - 1) * a + r for a (factor) and r (remainder)
        *factor = min_part_size / part_size;
        *internal_partitions = partitions / *factor;
        *remainder = partitions % (*internal_partitions);

        if (*remainder == 0) { // we still need the size of last partition
            *remainder = *factor;
        } else {               // number of partitions was floored, so add 1 for last (smaller) partition
            *internal_partitions += 1;
        }
    } else {    // can keep original partitioning
        *internal_partitions = partitions;
        *factor = 1;
        *remainder = 1;
    }

    // check if anything went wrong
    if (*internal_partitions > partitions || *factor < 1
          || ((*factor) * (*internal_partitions - 1) * part_size + (*remainder * part_size) != (partitions * part_size))) {
        opal_output_verbose(0, ompi_part_base_framework.framework_output, "given %lu*%lu partitioning and internal partitioning of %lu*%lu + %lu*%lu are inconsistent\n", partitions, part_size, *internal_partitions - 1, (*factor) * part_size, *remainder, part_size);
    }
}


/**
 * This is a helper function that frees a request. This requires ompi_part_persist_aggregated.lock be held before calling.
 */
__opal_attribute_always_inline__ static inline int
mca_part_persist_aggregated_free_req(struct mca_part_persist_aggregated_request_t* req)
{
    int err = OMPI_SUCCESS;
    size_t i;
    opal_list_remove_item(ompi_part_persist_aggregated.progress_list, (opal_list_item_t*)req->progress_elem);
    OBJ_RELEASE(req->progress_elem);

    // if on sender side, free aggregation state
    if (MCA_PART_persist_aggregated_REQUEST_PSEND == req->req_type) {
        mca_part_persist_aggregated_psend_request_t *sendreq = (mca_part_persist_aggregated_psend_request_t *) req;
        part_persist_aggregate_simple_free(&sendreq->aggregation_state);
    }

    for(i = 0; i < req->real_parts; i++) {
        ompi_request_free(&(req->persist_aggregated_reqs[i]));
    }
    free(req->persist_aggregated_reqs);
    free(req->flags);

    if( MCA_PART_persist_aggregated_REQUEST_PRECV == req->req_type ) {
        MCA_PART_persist_aggregated_PRECV_REQUEST_RETURN(req);
    } else {
        MCA_PART_persist_aggregated_PSEND_REQUEST_RETURN(req);
    }
    return err;
}


__opal_attribute_always_inline__ static inline void mca_part_persist_aggregated_init_lists(void)
{
    opal_free_list_init (&mca_part_base_precv_requests,
                         sizeof(mca_part_persist_aggregated_precv_request_t),
                         opal_cache_line_size,
                         OBJ_CLASS(mca_part_persist_aggregated_precv_request_t),
                         0,opal_cache_line_size,
                         ompi_part_persist_aggregated.free_list_num,
                         ompi_part_persist_aggregated.free_list_max,
                         ompi_part_persist_aggregated.free_list_inc,
                         NULL, 0, NULL, NULL, NULL);
    opal_free_list_init (&mca_part_base_psend_requests,
                         sizeof(mca_part_persist_aggregated_psend_request_t),
                         opal_cache_line_size,
                         OBJ_CLASS(mca_part_persist_aggregated_psend_request_t),
                         0,opal_cache_line_size,
                         ompi_part_persist_aggregated.free_list_num,
                         ompi_part_persist_aggregated.free_list_max,
                         ompi_part_persist_aggregated.free_list_inc,
                         NULL, 0, NULL, NULL, NULL);
     ompi_part_persist_aggregated.progress_list = OBJ_NEW(opal_list_t);
}

__opal_attribute_always_inline__ static inline void
mca_part_persist_aggregated_complete(struct mca_part_persist_aggregated_request_t* request)
{
    if(MCA_PART_persist_aggregated_REQUEST_PRECV == request->req_type) {
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
 * mca_part_persist_aggregated_progress is the progress function that will be registered. It handles 
 * both send and recv request testing and completion. It also handles freeing requests,
 * after MPI_Free is called and the requests have become inactive.
 */
__opal_attribute_always_inline__ static inline int
mca_part_persist_aggregated_progress(void)
{
    mca_part_persist_aggregated_list_t *current;
    int err;
    size_t i;

    /* prevent re-entry, */
    int block_entry = opal_atomic_add_fetch_32(&(ompi_part_persist_aggregated.block_entry), 1);
    if(1 < block_entry)
    {
        block_entry = opal_atomic_add_fetch_32(&(ompi_part_persist_aggregated.block_entry), -1);
        return OMPI_SUCCESS;
    }

    OPAL_THREAD_LOCK(&ompi_part_persist_aggregated.lock);
 
    mca_part_persist_aggregated_request_t* to_delete = NULL;

    /* Don't do anything till a function in the module is called. */
    if(-1 == ompi_part_persist_aggregated.init_world)
    {
        OPAL_THREAD_UNLOCK(&ompi_part_persist_aggregated.lock);
        block_entry = opal_atomic_add_fetch_32(&(ompi_part_persist_aggregated.block_entry), -1);
        return OMPI_SUCCESS;
    }

    /* Can't do anything if we don't have world */
    if(0 == ompi_part_persist_aggregated.init_world) {
        ompi_part_persist_aggregated.my_world_rank = ompi_comm_rank(&ompi_mpi_comm_world.comm);
        err = ompi_comm_idup(&ompi_mpi_comm_world.comm, &ompi_part_persist_aggregated.part_comm, &ompi_part_persist_aggregated.part_comm_req);
        if(err != OMPI_SUCCESS) {
             exit(-1);
        }
        ompi_part_persist_aggregated.part_comm_ready = 0;
        err = ompi_comm_idup(&ompi_mpi_comm_world.comm, &ompi_part_persist_aggregated.part_comm_setup, &ompi_part_persist_aggregated.part_comm_sreq);
        if(err != OMPI_SUCCESS) {
            exit(-1);
        }
        ompi_part_persist_aggregated.part_comm_sready = 0;
        ompi_part_persist_aggregated.init_world = 1;

        OPAL_THREAD_UNLOCK(&ompi_part_persist_aggregated.lock);
        block_entry = opal_atomic_add_fetch_32(&(ompi_part_persist_aggregated.block_entry), -1);
        return OMPI_SUCCESS;
    }

    /* Check to see if Comms are setup */
    if(0 == ompi_part_persist_aggregated.init_comms) {
        if(0 == ompi_part_persist_aggregated.part_comm_ready) {
            ompi_request_test(&ompi_part_persist_aggregated.part_comm_req, &ompi_part_persist_aggregated.part_comm_ready, MPI_STATUS_IGNORE);
        }
        if(0 == ompi_part_persist_aggregated.part_comm_sready) {
            ompi_request_test(&ompi_part_persist_aggregated.part_comm_sreq, &ompi_part_persist_aggregated.part_comm_sready, MPI_STATUS_IGNORE);
        }
        if(0 != ompi_part_persist_aggregated.part_comm_ready && 0 != ompi_part_persist_aggregated.part_comm_sready) {
            ompi_part_persist_aggregated.init_comms = 1;
        }
        OPAL_THREAD_UNLOCK(&ompi_part_persist_aggregated.lock);
        block_entry = opal_atomic_add_fetch_32(&(ompi_part_persist_aggregated.block_entry), -1);
        return OMPI_SUCCESS;
    }

    OPAL_LIST_FOREACH(current, ompi_part_persist_aggregated.progress_list, mca_part_persist_aggregated_list_t) {
        mca_part_persist_aggregated_request_t *req = (mca_part_persist_aggregated_request_t *) current->item;

        // get internal partitions that can be sent/queued
        if (MCA_PART_persist_aggregated_REQUEST_PSEND == req->req_type) 
        {
            mca_part_persist_aggregated_psend_request_t *sendreq = (mca_part_persist_aggregated_psend_request_t *) req;

            int flag_value;
            if(true == req->initialized) {
                flag_value = 0;     /* Mark partition as ready for testing */
            } else {
                flag_value = -2;    /* Mark partition as queued */
            }
        
            int internal_part;
            while(0 <= (internal_part = part_persist_aggregate_simple_pull(&sendreq->aggregation_state))) {
                if(true == req->initialized)
                    err = req->persist_aggregated_reqs[internal_part]->req_start(1, (&(req->persist_aggregated_reqs[internal_part])));
        
                req->flags[internal_part] = flag_value;
            }
        }

        /* Check to see if request is initialized */
        if(false == req->initialized) {
            int done = 0; 
            
            if(true == req->flag_post_setup_recv) {
                err = MCA_PML_CALL(irecv(&(req->setup_info[1]), sizeof(struct ompi_mca_persist_aggregated_setup_t), MPI_BYTE, OMPI_ANY_SOURCE, req->my_recv_tag, ompi_part_persist_aggregated.part_comm_setup, &req->setup_req[1]));
                req->flag_post_setup_recv = false;
            } 
 
            ompi_request_test(&(req->setup_req[1]), &done, MPI_STATUS_IGNORE);

            if(done) {
                size_t dt_size_;
                int32_t dt_size;

                if(MCA_PART_persist_aggregated_REQUEST_PSEND == req->req_type) {
                    /* parse message */
                    req->world_peer  = req->setup_info[1].world_rank; 

                    err = opal_datatype_type_size(&(req->req_datatype->super), &dt_size_);
                    if(OMPI_SUCCESS != err) return OMPI_ERROR;
                    dt_size = (dt_size_ > (size_t) INT_MAX) ? MPI_UNDEFINED : (int) dt_size_;
                    int32_t bytes = req->real_count * dt_size;

                    /* Set up persistent sends */
                    req->persist_aggregated_reqs = (ompi_request_t**) malloc(sizeof(ompi_request_t*)*(req->real_parts));
                    for(i = 0; i < req->real_parts - 1; i++) {
                         void *buf = ((void*) (((char*)req->req_addr) + (bytes * i)));
                         err = MCA_PML_CALL(isend_init(buf, req->real_count, req->req_datatype, req->world_peer, req->my_send_tag+i, MCA_PML_BASE_SEND_STANDARD, ompi_part_persist_aggregated.part_comm, &(req->persist_aggregated_reqs[i])));
                    }    
                    void *buf = ((void*) (((char*)req->req_addr) + (bytes * i)));
                    err = MCA_PML_CALL(isend_init(buf, req->real_remainder, req->req_datatype, req->world_peer, req->my_send_tag+i, MCA_PML_BASE_SEND_STANDARD, ompi_part_persist_aggregated.part_comm, &(req->persist_aggregated_reqs[i])));
                } else {
                    /* parse message */
                    req->world_peer     = req->setup_info[1].world_rank; 
                    req->my_send_tag    = req->setup_info[1].start_tag;
                    req->my_recv_tag    = req->setup_info[1].setup_tag;
                    req->real_parts     = req->setup_info[1].num_parts;
                    req->real_count     = req->setup_info[1].count;
                    req->real_remainder = req->setup_info[1].remainder;
                    req->real_dt_size   = req->setup_info[1].dt_size;

                    err = opal_datatype_type_size(&(req->req_datatype->super), &dt_size_);
                    if(OMPI_SUCCESS != err) return OMPI_ERROR;
                    dt_size = (dt_size_ > (size_t) INT_MAX) ? MPI_UNDEFINED : (int) dt_size_;
                    int32_t bytes = req->real_count * dt_size;



		            /* Set up persistent sends */
                    req->persist_aggregated_reqs = (ompi_request_t**) malloc(sizeof(ompi_request_t*)*(req->real_parts));
                    req->flags = (int*) calloc(req->real_parts,sizeof(int));

                    if(req->real_dt_size == dt_size) {

     	                for(i = 0; i < req->real_parts - 1; i++) {
                            void *buf = ((void*) (((char*)req->req_addr) + (bytes * i)));
                            err = MCA_PML_CALL(irecv_init(buf, req->real_count, req->req_datatype, req->world_peer, req->my_send_tag+i, ompi_part_persist_aggregated.part_comm, &(req->persist_aggregated_reqs[i])));
                        }
                        void *buf = ((void*) (((char*)req->req_addr) + (bytes * i)));
                        err = MCA_PML_CALL(irecv_init(buf, req->real_remainder, req->req_datatype, req->world_peer, req->my_send_tag+i, ompi_part_persist_aggregated.part_comm, &(req->persist_aggregated_reqs[i])));
                    } else {
                        for(i = 0; i < req->real_parts - 1; i++) {
                            void *buf = ((void*) (((char*)req->req_addr) + (req->real_count * req->real_dt_size * i)));
                            err = MCA_PML_CALL(irecv_init(buf, req->real_count * req->real_dt_size, MPI_BYTE, req->world_peer, req->my_send_tag+i, ompi_part_persist_aggregated.part_comm, &(req->persist_aggregated_reqs[i])));
                        }
                        void *buf = ((void*) (((char*)req->req_addr) + (req->real_count * req->real_dt_size * i)));
                        err = MCA_PML_CALL(irecv_init(buf, req->real_remainder * req->real_dt_size, MPI_BYTE, req->world_peer, req->my_send_tag+i, ompi_part_persist_aggregated.part_comm, &(req->persist_aggregated_reqs[i])));
		            }

                    err = req->persist_aggregated_reqs[0]->req_start(req->real_parts, (&(req->persist_aggregated_reqs[0])));                     

                    /* Send back a message */
                    req->setup_info[0].world_rank = ompi_part_persist_aggregated.my_world_rank;
                    err = MCA_PML_CALL(isend(&(req->setup_info[0]), sizeof(struct ompi_mca_persist_aggregated_setup_t), MPI_BYTE, req->world_peer, req->my_recv_tag, MCA_PML_BASE_SEND_STANDARD, ompi_part_persist_aggregated.part_comm_setup, &req->setup_req[0]));
                    if(OMPI_SUCCESS != err) return OMPI_ERROR;
                }

                req->initialized = true; 
            }
        } else {
            if(false == req->req_part_complete && REQUEST_COMPLETED != req->req_ompi.req_complete && OMPI_REQUEST_ACTIVE == req->req_ompi.req_state) {
               for(i = 0; i < req->real_parts; i++) {

                    /* Check to see if partition is queued for being started. Only applicable to sends. */ 
                    if(-2 ==  req->flags[i]) {
                        err = req->persist_aggregated_reqs[i]->req_start(1, (&(req->persist_aggregated_reqs[i])));
                        req->flags[i] = 0;
                    }

                    if(0 == req->flags[i])
                    {
                        ompi_request_test(&(req->persist_aggregated_reqs[i]), &(req->flags[i]), MPI_STATUS_IGNORE);
                        if(0 != req->flags[i]) req->done_count++;
                    }
                }

                /* Check for completion and complete the requests */
                if(req->done_count == req->real_parts)
                {
                    req->first_send = false;
                    mca_part_persist_aggregated_complete(req);
                } 
            }

            if(true == req->req_free_called && true == req->req_part_complete && REQUEST_COMPLETED == req->req_ompi.req_complete &&  OMPI_REQUEST_INACTIVE == req->req_ompi.req_state) {
                to_delete = req;
            }
        }

    }
    OPAL_THREAD_UNLOCK(&ompi_part_persist_aggregated.lock);
    block_entry = opal_atomic_add_fetch_32(&(ompi_part_persist_aggregated.block_entry), -1);
    if(to_delete) {
        err =  mca_part_persist_aggregated_free_req(to_delete);
        if (OMPI_SUCCESS != err) {
            return OMPI_ERROR;
        }
    }

    return OMPI_SUCCESS;
}

__opal_attribute_always_inline__ static inline int
mca_part_persist_aggregated_precv_init(void *buf,
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
    mca_part_persist_aggregated_list_t* new_progress_elem = NULL;

    mca_part_persist_aggregated_precv_request_t *recvreq;

    /* if module hasn't been called before, flag module to init. */
    if(-1 == ompi_part_persist_aggregated.init_world)
    {
        ompi_part_persist_aggregated.init_world = 0;
    }

    /* Allocate a new request */
    MCA_PART_persist_aggregated_PRECV_REQUEST_ALLOC(recvreq);
    if (OPAL_UNLIKELY(NULL == recvreq)) return OMPI_ERR_OUT_OF_RESOURCE;

    MCA_PART_persist_aggregated_PRECV_REQUEST_INIT(recvreq, ompi_proc, comm, tag, src,
                                     datatype, buf, parts, count, flags);

    mca_part_persist_aggregated_request_t *req = (mca_part_persist_aggregated_request_t *) recvreq;

    /* Set lazy initializion flags */
    req->initialized = false;
    req->first_send  = true; 
    req->flag_post_setup_recv = false;
    req->flags = NULL;
    /* Non-blocking receive on setup info */
    err	= MCA_PML_CALL(irecv(&req->setup_info[1], sizeof(struct ompi_mca_persist_aggregated_setup_t), MPI_BYTE, src, tag, comm, &req->setup_req[1])); 
    if(OMPI_SUCCESS != err) return OMPI_ERROR;

    /* Compute total number of bytes */
    err = opal_datatype_type_size(&(req->req_datatype->super), &dt_size_);
    if(OMPI_SUCCESS != err) return OMPI_ERROR;
    dt_size = (dt_size_ > (size_t) INT_MAX) ? MPI_UNDEFINED : (int) dt_size_;
    req->req_bytes = parts * count * dt_size;

    /* Set ompi request initial values */
    req->req_ompi.req_persistent = true;
    req->req_part_complete = true;
    req->req_ompi.req_complete = REQUEST_COMPLETED;
    req->req_ompi.req_state = OMPI_REQUEST_INACTIVE;

    /* Add element to progress engine */
    new_progress_elem = OBJ_NEW(mca_part_persist_aggregated_list_t);
    new_progress_elem->item = req;
    req->progress_elem = new_progress_elem; 
    OPAL_THREAD_LOCK(&ompi_part_persist_aggregated.lock);
    opal_list_append(ompi_part_persist_aggregated.progress_list, (opal_list_item_t*)new_progress_elem);
    OPAL_THREAD_UNLOCK(&ompi_part_persist_aggregated.lock);

    /* set return values */
    *request = (ompi_request_t*) recvreq;
    return err;
}

__opal_attribute_always_inline__ static inline int
mca_part_persist_aggregated_psend_init(const void* buf,
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
    mca_part_persist_aggregated_list_t* new_progress_elem = NULL;
    mca_part_persist_aggregated_psend_request_t *sendreq;

    /* if module hasn't been called before, flag module to init. */
    if(-1 == ompi_part_persist_aggregated.init_world)
    {
        ompi_part_persist_aggregated.init_world = 0;
    }

    /* Create new request object */
    MCA_PART_persist_aggregated_PSEND_REQUEST_ALLOC(sendreq, comm, dst, ompi_proc);
    if (OPAL_UNLIKELY(NULL == sendreq)) return OMPI_ERR_OUT_OF_RESOURCE;
    MCA_PART_persist_aggregated_PSEND_REQUEST_INIT(sendreq, ompi_proc, comm, tag, dst,
                                    datatype, buf, parts, count, flags);
    mca_part_persist_aggregated_request_t *req = (mca_part_persist_aggregated_request_t *) sendreq;

    /* Set lazy initialization variables */
    req->initialized = false;
    req->first_send  = true; 
    

    /* Determine total bytes to send. */
    err = opal_datatype_type_size(&(req->req_datatype->super), &dt_size_);
    if(OMPI_SUCCESS != err) return OMPI_ERROR;
    dt_size = (dt_size_ > (size_t) INT_MAX) ? MPI_UNDEFINED : (int) dt_size_;
    req->req_bytes = parts * count * dt_size;

    // select internal partitioning (i.e. real_parts) here
    size_t factor, remaining_partitions;
    part_persist_aggregated_select_internal_partitioning(parts, count, &req->real_parts, &factor, &remaining_partitions);

    req->real_remainder = remaining_partitions * count;     // convert to number of elements
    req->real_count = factor * count;
    req->setup_info[0].num_parts = req->real_parts;         // setup info has to contain internal partitioning
    req->setup_info[0].count     = req->real_count;
    req->setup_info[0].remainder = req->real_remainder;
    opal_output_verbose(5, ompi_part_base_framework.framework_output, "mapped given %lu*%lu partitioning to internal partitioning of %lu*%lu + %lu\n", parts, count, req->real_parts - 1, req->real_count, req->real_remainder);

    // init aggregation state
    part_persist_aggregate_simple_init(&sendreq->aggregation_state, req->real_parts, factor, remaining_partitions);

    /* non-blocking send set-up data */
    req->setup_info[0].dt_size = dt_size;
    req->setup_info[0].world_rank = ompi_comm_rank(&ompi_mpi_comm_world.comm);
    req->setup_info[0].start_tag = ompi_part_persist_aggregated.next_send_tag; 
    ompi_part_persist_aggregated.next_send_tag += req->real_parts; 
    req->my_send_tag = req->setup_info[0].start_tag;
    req->setup_info[0].setup_tag = ompi_part_persist_aggregated.next_recv_tag; 
    ompi_part_persist_aggregated.next_recv_tag++;
    req->my_recv_tag = req->setup_info[0].setup_tag;

    req->flags = (int*) calloc(req->real_parts, sizeof(int));

    err = MCA_PML_CALL(isend(&(req->setup_info[0]), sizeof(struct ompi_mca_persist_aggregated_setup_t), MPI_BYTE, dst, tag, MCA_PML_BASE_SEND_STANDARD, comm, &req->setup_req[0]));
    if(OMPI_SUCCESS != err) return OMPI_ERROR;

    /* Non-blocking receive on setup info */
    if(1 == ompi_part_persist_aggregated.init_comms) {
        err = MCA_PML_CALL(irecv(&(req->setup_info[1]), sizeof(struct ompi_mca_persist_aggregated_setup_t), MPI_BYTE, MPI_ANY_SOURCE, req->my_recv_tag, ompi_part_persist_aggregated.part_comm_setup, &req->setup_req[1]));
        if(OMPI_SUCCESS != err) return OMPI_ERROR;
        req->flag_post_setup_recv = false;
    } else {
        req->flag_post_setup_recv = true;
    }

    /* Initilaize completion variables */
    sendreq->req_base.req_ompi.req_persistent = true;
    req->req_part_complete = true;
    req->req_ompi.req_complete = REQUEST_COMPLETED;
    req->req_ompi.req_state = OMPI_REQUEST_INACTIVE;
 
    /* add element to progress queue */
    new_progress_elem = OBJ_NEW(mca_part_persist_aggregated_list_t);
    new_progress_elem->item = req;
    req->progress_elem = new_progress_elem;
    OPAL_THREAD_LOCK(&ompi_part_persist_aggregated.lock);
    opal_list_append(ompi_part_persist_aggregated.progress_list, (opal_list_item_t*)new_progress_elem);
    OPAL_THREAD_UNLOCK(&ompi_part_persist_aggregated.lock);

    /* Set return values */
    *request = (ompi_request_t*) sendreq;
    return err;
}

__opal_attribute_always_inline__ static inline int
mca_part_persist_aggregated_start(size_t count, ompi_request_t** requests)
{
    int err = OMPI_SUCCESS;
    size_t _count = count;
    size_t i;

    for(i = 0; i < _count && OMPI_SUCCESS == err; i++) {
        mca_part_persist_aggregated_request_t *req = (mca_part_persist_aggregated_request_t *)(requests[i]);

        // reset aggregation state here
        if (MCA_PART_persist_aggregated_REQUEST_PSEND == req->req_type) {
            mca_part_persist_aggregated_psend_request_t *sendreq = (mca_part_persist_aggregated_psend_request_t *)(req);
            part_persist_aggregate_simple_reset(&sendreq->aggregation_state);
        }

        /* First use is a special case, to support lazy initialization */
        if(false == req->first_send)
        {
            if(MCA_PART_persist_aggregated_REQUEST_PSEND == req->req_type) {
                req->done_count = 0;
                memset((void*)req->flags,0,sizeof(int32_t)*req->real_parts);
            } else {
                req->done_count = 0;
                err = req->persist_aggregated_reqs[0]->req_start(req->real_parts, req->persist_aggregated_reqs);
                memset((void*)req->flags,0,sizeof(int32_t)*req->real_parts);
            } 
        } else {
            if(MCA_PART_persist_aggregated_REQUEST_PSEND == req->req_type) {
                req->done_count = 0;
                for(i = 0; i < req->real_parts && OMPI_SUCCESS == err; i++) {
                    req->flags[i] = -1;
                }
            } else {
                req->done_count = 0;
            } 
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
mca_part_persist_aggregated_pready(size_t min_part,
                    size_t max_part,
                    ompi_request_t* request)
{
    int err = OMPI_SUCCESS;

    size_t i;
    mca_part_persist_aggregated_request_t *req = (mca_part_persist_aggregated_request_t *)(request);

    if (MCA_PART_persist_aggregated_REQUEST_PSEND != req->req_type) {
        return OMPI_ERROR;
    }

    // 
    mca_part_persist_aggregated_psend_request_t *sendreq = (mca_part_persist_aggregated_psend_request_t *)(req);
    for(i = min_part; i <= max_part && OMPI_SUCCESS == err; i++) {
        part_persist_aggregate_simple_push(&sendreq->aggregation_state, i);
    }
      
    return err;
}

__opal_attribute_always_inline__ static inline int
mca_part_persist_aggregated_parrived(size_t min_part,
                      size_t max_part,
                      int* flag, 
                      ompi_request_t* request)
{
    int err = OMPI_SUCCESS;
    size_t i;
    int _flag = false;
    mca_part_persist_aggregated_request_t *req = (mca_part_persist_aggregated_request_t *)request;

    if(0 != req->flags) {
        _flag = 1;
        if(req->req_parts == req->real_parts) {
            for(i = min_part; i <= max_part; i++) {
                _flag = _flag && req->flags[i];            
            }
        } else {
            float convert = ((float)req->real_parts) / ((float)req->req_parts);
            size_t _min = floor(convert * min_part);
            size_t _max = ceil(convert * max_part);
            for(i = _min; i <= _max; i++) {
                _flag = _flag && req->flags[i];
            }
        }
    }

    if(!_flag) {
        opal_progress();
    } 
    *flag = _flag;
    return err;
}


/**
 * mca_part_persist_aggregated_free marks an entry as free called and sets the request to 
 * MPI_REQUEST_NULL. Note: requests get freed in the progress engine. 
 */
__opal_attribute_always_inline__ static inline int
mca_part_persist_aggregated_free(ompi_request_t** request)
{
    mca_part_persist_aggregated_request_t* req = *(mca_part_persist_aggregated_request_t**)request;

    if(true == req->req_free_called) return OMPI_ERROR;
    req->req_free_called = true;

    *request = MPI_REQUEST_NULL;
    return OMPI_SUCCESS;
}

END_C_DECLS

#endif  /* PART_persist_aggregated_H_HAS_BEEN_INCLUDED */

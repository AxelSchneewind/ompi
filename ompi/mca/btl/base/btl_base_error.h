/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007-2008 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007      Sun Microsystems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef MCA_BTL_BASE_ERROR_H
#define MCA_BTL_BASE_ERROR_H

#include "ompi_config.h"

#include <errno.h>
#include <stdio.h>

#include "orte/util/proc_info.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

OMPI_DECLSPEC extern int mca_btl_base_verbose;

OMPI_DECLSPEC extern int mca_btl_base_err(const char*, ...);
OMPI_DECLSPEC extern int mca_btl_base_out(const char*, ...);

#define BTL_OUTPUT(args)                                     \
do {                                                         \
    mca_btl_base_out("[%s]%s[%s:%d:%s] ",                    \
            orte_proc_info.nodename,                         \
            orte_util_print_name_args(ORTE_PROC_MY_NAME),       \
            __FILE__, __LINE__, __func__);                   \
    mca_btl_base_out args;                                   \
    mca_btl_base_out("\n");                                  \
} while(0);


#define BTL_ERROR(args)                                      \
do {                                                         \
    mca_btl_base_err("[%s]%s[%s:%d:%s] ",                    \
            orte_proc_info.nodename,                         \
            orte_util_print_name_args(ORTE_PROC_MY_NAME),       \
            __FILE__, __LINE__, __func__);                   \
    mca_btl_base_err args;                                   \
    mca_btl_base_err("\n");                                  \
} while(0);

#define BTL_PEER_ERROR(proc, args)                               \
do {                                                             \
    mca_btl_base_err("%s[%s:%d:%s] from %s ",                    \
                     orte_util_print_name_args(ORTE_PROC_MY_NAME),  \
                     __FILE__, __LINE__, __func__,               \
                     orte_proc_info.nodename);                   \
    if(proc && proc->proc_hostname) {                            \
        mca_btl_base_err("to: %s ", proc->proc_hostname);        \
    }                                                            \
    mca_btl_base_err args;                                       \
    mca_btl_base_err("\n");                                      \
} while(0);


#if OMPI_ENABLE_DEBUG
#define BTL_VERBOSE(args)                                    \
do {                                                         \
   if(mca_btl_base_verbose > 0) {                            \
        mca_btl_base_err("[%s]%s[%s:%d:%s] ",                \
                orte_proc_info.nodename,                     \
                orte_util_print_name_args(ORTE_PROC_MY_NAME),  \
                __FILE__, __LINE__, __func__);               \
        mca_btl_base_err args;                               \
        mca_btl_base_err("\n");                              \
   }                                                         \
} while(0); 
#else
#define BTL_VERBOSE(args) 
#endif

#endif


BEGIN_C_DECLS

OMPI_DECLSPEC extern void mca_btl_base_error_no_nics(const char* transport, 
                                                     const char* nic_name);

END_C_DECLS

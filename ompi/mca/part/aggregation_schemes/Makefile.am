#
# Copyright (c) 2004-2006 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2009      High Performance Computing Center Stuttgart,
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2017      IBM Corporation.  All rights reserved.
# Copyright (c) 2020-2021 Sandia National Laboratories. All rights reserved.
# Copyright (c) 2026      High Performance Computing Center Stuttgart,
#                         University of Stuttgart.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

EXTRA_DIST = post_configure.sh

if MCA_BUILD_ompi_part_aggregation_schemes_DSO
component_noinst =
component_install = mca_part_aggregation_schemes.la
else
component_noinst = libmca_part_aggregation_schemes.la
component_install =
endif

local_sources = \
	aggregation_scheme_regular.c \
	aggregation_scheme_regular.h

mcacomponentdir = $(ompilibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_part_aggregation_schemes_la_SOURCES = $(local_sources)
mca_part_aggregation_schemes_la_LIBADD = $(top_builddir)/ompi/lib@OMPI_LIBMPI_NAME@.la \
	$(aggregation_schemes_LIBS)
mca_part_aggregation_schemes_la_LDFLAGS = -module -avoid-version $(aggregation_schemes_LDFLAGS)

noinst_LTLIBRARIES = $(component_noinst)
libmca_part_aggregation_schemes_la_SOURCES = $(local_sources)
libmca_part_aggregation_schemes_la_LIBADD = $(aggregation_schemes_LIBS)
libmca_part_aggregation_schemes_la_LDFLAGS = -module -avoid-version $(aggregation_schemes_LDFLAGS)


#
# Copyright (c) 2024      Huawei Technologies Co., Ltd.
#                         All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

AM_CPPFLAGS = $(ras_donau_CPPFLAGS)

dist_prtedata_DATA = help-ras-donau.txt
sources = \
        ras_donau.h \
        ras_donau_component.c \
        ras_donau_module.c


# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_prte_ras_donau_DSO
lib =
lib_sources =
component = prte_mca_ras_donau.la
component_sources = $(sources)
else
lib = libprtemca_ras_donau.la
lib_sources = $(sources)
component =
component_sources =
endif

mcacomponentdir = $(prtelibdir)
mcacomponent_LTLIBRARIES = $(component)
prte_mca_ras_donau_la_SOURCES = $(component_sources)
prte_mca_ras_donau_la_LIBADD = ${ras_donau_LIBS} \
                                $(top_builddir)/src/libprrte.la
prte_mca_ras_donau_la_LDFLAGS = -module -avoid-version $(ras_donau_LDFLAGS)

noinst_LTLIBRARIES = $(lib)
libprtemca_ras_donau_la_SOURCES = $(lib_sources)
libprtemca_ras_donau_la_LDFLAGS = -module -avoid-version $(ras_donau_LDFLAGS)
libprtemca_ras_donau_la_LIBADD = $(ras_donau_LIBS)
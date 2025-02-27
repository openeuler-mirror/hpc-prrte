#
# Copyright (c) 2024      Huawei Technologies Co., Ltd.
#                         All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        plm_donau.h \
        plm_donau_component.c \
        plm_donau_module.c

dist_prtedata_DATA = help-plm-donau.txt

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_prte_plm_donau_DSO
component_noinst = 
component_install = prte_mca_plm_donau.la
else
component_noinst = libprtemca_plm_donau.la
component_install =
endif

mcacomponentdir = $(prtelibdir)
mcacomponent_LTLIBRARIES = $(component_install)
prte_mca_plm_donau_la_SOURCES = $(sources)
prte_mca_plm_donau_la_LDFLAGS = -module -avoid-version
prte_mca_plm_donau_la_LIBADD = $(top_builddir)/src/libprrte.la

noinst_LTLIBRARIES = $(component_noinst)
libprtemca_plm_donau_la_SOURCES = $(sources)
libprtemca_plm_donau_la_LDFLAGS = -module -avoid-version
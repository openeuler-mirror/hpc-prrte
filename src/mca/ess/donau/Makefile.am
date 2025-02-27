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
        ess_donau.h \
        ess_donau_component.c \
        ess_donau_module.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_prte_ess_donau_DSO
component_noinst =
component_install = prte_mca_ess_donau.la
else
component_noinst = libprtemca_ess_donau.la
component_instal =
endif

mcacomponentdir = $(prtelibdir)
mcacomponent_LTLIBRARIES = $(component_install)
prte_mca_ess_donau_la_SOURCES = $(sources)
prte_mca_ess_donau_la_LDFLAGS = -module -avoid-version
prte_mca_ess_donau_la_LIBADD = $(top_builddir)/src/libprrte.la

noinst_LTLIBRARIES = $(component_noinst)
libprtemca_ess_donau_la_SOURCES = $(sources)
libprtemca_ess_donau_la_LDFLAGS = -module -avoid-version
# -*- shell-script -*-
#
# Copyright (c) 2024      Huawei Technologies Co., Ltd.
#                         All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# MCA_plm_donau_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_prte_plm_donau_CONFIG],[
    AC_CONFIG_FILES([src/mca/plm/donau/Makefile])

    PRTE_CHECK_DONAU([plm_donau], [plm_donau_good=1], [plm_donau_good=0])

    # if check worked, set wrapper flags if so.
    # Evaluate succeed / fail
    AS_IF([test "$plm_donau_good" = "1"],
          [$1],
          [$2])

    # set build flags to use in makefile
    AC_SUBST([plm_donau_CPPFLAGS])
    AC_SUBST([plm_donau_LDFLAGS])
    AC_SUBST([plm_donau_LIBS])

])dnl
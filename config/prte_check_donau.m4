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

# PRTE_CHECK_DONAU(prefix, [action-if-found], [action-if-not-found])
# --------------------------------------------------------
AC_DEFUN([PRTE_CHECK_DONAU],[
    if test -z "$prte_check_donau_happy" ; then
        AC_ARG_WITH([donau],
                [AC_HELP_STRING([--with-donau],
                                [Build DONAU scheduler component (default: yes)])])
        if test "$with_donau" = "no" ; then
            prte_check_donau_happy="no"
        else
            prte_check_donau_happy="yes"
        fi

        AS_IF([test "$prte_check_donau_happy" = "yes"],
              [AC_CHECK_FUNC([fork],
                             [prte_check_donau_happy="yes"],
                             [prte_check_donau_happy="no"])])

        AS_IF([test "$prte_check_donau_happy" = "yes"],
              [AC_CHECK_FUNC([execve],
                             [prte_check_donau_happy="yes"],
                             [prte_check_donau_happy="no"])])

        AS_IF([test "$prte_check_donau_happy" = "yes"],
              [AC_CHECK_FUNC([setpgid],
                             [prte_check_donau_happy="yes"],
                             [prte_check_donau_happy="no"])])
    fi
    AS_IF([test "$prte_check_donau_happy" = "yes"],
          [$2],
          [$3])
])
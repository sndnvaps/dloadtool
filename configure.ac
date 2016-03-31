#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([dloadtool], [0.2b], [jnaulet@rdinnovation.fr])
AC_CONFIG_SRCDIR([ihex.c])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lcintelhex':
AC_CHECK_LIB([cintelhex], [ihex_rs_from_file])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

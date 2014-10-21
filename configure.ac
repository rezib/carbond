AC_PREREQ([2.69])
AC_INIT(carbond, 0.1, carbond@github.com)
AM_INIT_AUTOMAKE(carbond, 0.1)
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for library functions.
AC_SEARCH_LIBS([pthread_create],[pthread])
AC_SEARCH_LIBS([clock_gettime],[rt posix4])

# Checks for libraries.
PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES([PCRE], [libpcre])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 etc/Makefile])

AC_OUTPUT

dnl Process this file with autoconf to produce a configure script.
AC_INIT(src/rapi.c)

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE(librapi, 0.9.3)
AM_CONFIG_HEADER(rapi_config.h)

CFLAGS="-Wall -Wsign-compare -Wno-long-long $CFLAGS"

case $target in

	powerpc-apple-*)
		dnl Prevent "Undefined Macro argument list" error.
		CFLAGS="-no-cpp-precomp $CFLAGS"
		;;

	*)
		;;

esac

dnl Checks for typedefs, structures, and compiler characteristics.
saved_CFLAGS="$CFLAGS"
CFLAGS=""
AC_C_CONST
CFLAGS="$saved_CFLAGS"

AC_CHECK_SIZEOF(void *)

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
dnl AM_PATH_CHECK(,,AC_MSG_WARN([Check not available.]))

dnl Check for dmalloc first
AC_ARG_WITH(dmalloc,
		AC_HELP_STRING([--with-dmalloc=PREFIX],[use dmalloc (default is NO)]),
[case "${withval}" in
  yes) dmalloc=true ;;
  no)  dmalloc=false ;;
  *) dmalloc=true; dmalloc_prefix="-L${withval}" ;;
esac],[dmalloc=false])

if ${dmalloc}; then
	AC_CHECK_LIB(dmalloc,main,,[
			AC_MSG_ERROR([Can't find dmalloc library])
		],${dmalloc_prefix})
	AC_CHECK_HEADERS(dmalloc.h,,[
			AC_MSG_ERROR([Can't find dmalloc.h])
		])
fi

dnl Checks for libraries.
AC_LIB_RPATH

dnl Check for libsynce
PKG_CHECK_MODULES(LIBSYNCE, libsynce >= 0.9.1)

dnl Checks for header files.
dnl For readv/writev
AC_CHECK_HEADERS(sys/uio.h)

dnl Checks for library functions.
AC_CHECK_FUNCS(readv)
AC_CHECK_FUNCS(writev)

AC_CONFIG_FILES(
	[script/synce-install-cab],
	[chmod +x script/synce-install-cab]
)

AC_CONFIG_FILES(
	[script/synce-remove-program],
	[chmod +x script/synce-remove-program]
)

AC_OUTPUT([librapi2.pc
           Makefile
           src/Makefile
           src/support/Makefile
           src/config/Makefile
           src/rapi/Makefile
           src/rapi2/Makefile
           script/Makefile
           tests/Makefile
           tests/CeRapiInvoke/Makefile
           tests/rapi/Makefile
           tools/Makefile
           tools/man/Makefile])


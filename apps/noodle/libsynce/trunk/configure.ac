dnl Process this file with autoconf to produce a configure script.
AC_INIT(lib/synce.h.in)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE(libsynce, 0.9.3)
AM_CONFIG_HEADER(lib/synce_config.h)

CFLAGS="-Wall -Wsign-compare -Wno-long-long $CFLAGS"

case $target in

	powerpc-apple-*)
		dnl Prevent "Undefined Macro argument list" error.
		CFLAGS="-no-cpp-precomp $CFLAGS"
		;;

esac

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
dnl AM_PATH_CHECK()

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

use_dbus=0
AC_ARG_ENABLE(dbus,
              AC_HELP_STRING([--enable-dbus],[Include D-BUS support]),[
PKG_CHECK_MODULES(DBUS, "dbus-1",
                  [use_dbus=1])
],[use_dbus=0])
AC_DEFINE_UNQUOTED(USE_DBUS,$use_dbus,[Defined if D-BUS is available.])

dnl Checks for header files.

dnl Big-endian stuff
AC_SUBST(WORDS_BIGENDIAN)
AC_SUBST(BYTESWAP_HEADER,dummy)
AC_SUBST(HAVE_BYTESWAP_HEADER,0)

AC_C_BIGENDIAN()

if test "$ac_cv_c_bigendian" = yes; then
  WORDS_BIGENDIAN=1
  AC_CHECK_HEADERS(sys/byteswap.h,[BYTESWAP_HEADER=sys/byteswap.h; HAVE_BYTESWAP_HEADER=1])
  AC_CHECK_HEADERS(byteswap.h,    [BYTESWAP_HEADER=byteswap.h;     HAVE_BYTESWAP_HEADER=1])
else
  WORDS_BIGENDIAN=0
fi

dnl Check where uint16_t and uint32_t may be found
AC_SUBST(INT_HEADER)

dnl Old compilers
AC_CHECK_HEADERS(inttypes.h,[INT_HEADER=inttypes.h])

dnl C99 compliant compilers
AC_CHECK_HEADERS(stdint.h,[INT_HEADER=stdint.h])

if test -z "$INT_HEADER"; then
	AC_MSG_ERROR([Don't know how to get uint16_t and uint32_t on your system])
fi

AC_CHECK_HEADERS(stdbool.h,,[
  AC_MSG_ERROR([Don't know how to get bool values on your system])
])

dnl For setlocale
AC_CHECK_HEADERS(locale.h)
dnl For nl_langinfo
AC_CHECK_HEADERS(langinfo.h)

dnl Checks for typedefs, structures, and compiler characteristics.
saved_CFLAGS="$CFLAGS"
CFLAGS=""
AC_C_CONST
CFLAGS="$saved_CFLAGS"

dnl Checks for library functions.
AC_CHECK_FUNCS(nl_langinfo)
AC_CHECK_FUNCS(setlocale)
AC_CHECK_FUNCS(poll)
AC_REPLACE_FUNCS(inet_pton)

# http://www.gnu.org/manual/autoconf-2.53/html_node/AC_LIBOBJ-vs.-LIBOBJS.html
# This is necessary so that .o files in LIBOBJS are also built via
# the ANSI2KNR-filtering rules.
LIB@&t@OBJS=`echo "$LIB@&t@OBJS" | sed 's,\.[[^.]]* ,$U&,g;s,\.[[^.]]*$,$U&,'`
LTLIBOBJS=`echo "$LIB@&t@OBJS" | sed 's,\.[[^.]]* ,.lo ,g;s,\.[[^.]]*$,.lo,'`
AC_SUBST(LTLIBOBJS)


AM_ICONV()
if test "$am_cv_func_iconv" '!=' yes; then
	AC_MSG_ERROR([iconv not found, try using --with-libiconv-prefix])
fi
AC_CHECK_HEADERS(iconv.h,,[
		AC_MSG_WARN([iconv.h not found (INCICONV=$INCICONV)])
		])

AC_CHECK_LIB(socket,main)
AC_CHECK_LIB(nsl,main)

AC_CONFIG_FILES( [lib/synce.h] )

AC_OUTPUT([libsynce.pc
           Makefile
           lib/Makefile
					 lib/config/Makefile
					 man/Makefile])


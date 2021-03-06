# Even minor version = development, odd minor version = stable
AC_INIT([Read Route Record],[1.17-1])
AC_DEFINE([RRR_CONFIG_VERSION], "1.17-1", [Full version])
AC_DEFINE([RRR_CONFIG_VERSION_MAJOR], 1, [Major version])
AC_DEFINE([RRR_CONFIG_VERSION_MINOR], 17, [Minor version])
AC_DEFINE([RRR_CONFIG_VERSION_REVISION], 1, [Revision number])
AC_DEFINE([RRR_TYPE_MAX_BLOB_LENGTH], 1024, [Maximum size of blob type in type arrays])
#AC_DEFINE([RRR_MYSQL_BIND_MAX], 16, [Maximum number of bind columns for MySQL])
AC_DEFINE([RRR_TMP_PATH], "/tmp", [Temporary directory])
AC_DEFINE([RRR_MESSAGE_MIME_TYPE], "application/rrr-message", [MIME type of RRR message])
AC_DEFINE([RRR_STATS_SOCKET_PREFIX], "rrr_stats", [Socket name prefix for RRR stats socket])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_MACRO_DIRS([m4])

AC_CONFIG_SUBDIRS([src/blockdevlogger])

INET_CHECKS()

dnl OpenBSD check for egcc
AC_CHECK_PROG([EGCC], egcc, egcc)
AC_MSG_CHECKING([for OpenBSD EGCC])
AS_IF([test "x${EGCC}" != "x"], [
		AS_IF([test "x${CC}" != "x" ], [
		AC_MSG_RESULT([found, but CC was already set to something (else)])
	], [
		CC=${EGCC}
		AC_MSG_RESULT([${EGCC}])
	])
], [
	AC_MSG_RESULT([no])
])

AM_INIT_AUTOMAKE([subdir-objects no-dependencies])
LT_INIT
AC_PROG_CC
AC_PROG_INSTALL
#AC_PROG_YACC
#AM_PROG_LEX
AM_SILENT_RULES([yes])

echo "#define RRR_BUILD_TIMESTAMP " `date +%Y%m%d%H%M%S` > build_timestamp.h
echo "#define RRR_BUILD_DIR \""`pwd`"\"" > build_directory.h

AC_CHECK_HEADERS([pthread.h sys/un.h linux/un.h endian.h])
AC_CHECK_FUNCS(vsnprintf vasprintf strcasestr strlcpy)

AC_ARG_WITH([code-coverage], [AS_HELP_STRING([--with-code-coverage], [Enable code coverage compilation])], [enable_codecov=yes], [enable_codecov=no])
AC_ARG_WITH([eventfd],[AS_HELP_STRING([--without-eventfd],[build without the eventfd bindings event when available])],[enable_eventfd=no],[enable_eventfd=yes])
AC_ARG_WITH([encryption], [AS_HELP_STRING([--without-encryption], [Disable LibreSSL/OpenSSL password encryption])], [enable_encryption=no], [enable_encryption=yes])
AC_ARG_WITH([autossl], [AS_HELP_STRING([--without-autossl], [Disable automatic LibreSSL/OpenSSL support])], [enable_auto_ssl=no], [enable_auto_ssl=yes])
AC_ARG_WITH([libressl], [AS_HELP_STRING([--with-libressl], [Force enable LibreSSL support])], [enable_libressl=yes; enable_auto_ssl=no], [ enable_libressl=no ])
AC_ARG_WITH([openssl-ish], [AS_HELP_STRING([--with-openssl-ish], [Force enable OpenSSL (or LibreSSL-style OpenSSL) support])], [enable_openssl=yes; enable_auto_ssl=no], [ enable_openssl=no ])
AC_ARG_WITH([jsonc],[AS_HELP_STRING([--without-jsonc],[build without the json-c bindings])],[enable_jsonc=no],[enable_jsonc=yes])
AC_ARG_WITH([nghttp2],[AS_HELP_STRING([--without-nghttp2],[build without the NGHTTP2 bindings])],[enable_nghttp2=no],[enable_nghttp2=yes])
AC_ARG_WITH([mysql],[AS_HELP_STRING([--without-mysql],[build without the MySQL bindings])],[enable_mysql=no],[enable_mysql=yes])
AC_ARG_WITH([perl5],[AS_HELP_STRING([--without-perl5],[build without the Perl5 bindings])],[enable_perl5=no],[enable_perl5=yes])
AC_ARG_WITH([usb],[AS_HELP_STRING([--with-usb],[build with USB bindings])],[enable_usb=yes],[enable_usb=no])
AC_ARG_WITH([python3],[AS_HELP_STRING([--without-python3],[build without the python bindings])],[enable_python3=no],[enable_python3=yes])
AC_ARG_WITH([systemd],[AS_HELP_STRING([--without-systemd],[build without the systemd bindings])],[enable_systemd=no],[enable_systemd=yes])
AC_ARG_WITH([runit],[AS_HELP_STRING([--with-runit],[build with the runit bindings])],[enable_runit=yes],[enable_runit=no])
AC_ARG_WITH([openrc],[AS_HELP_STRING([--with-openrc],[build with the OpenRC bindings])],[enable_openrc=yes],[enable_openrc=no])
AC_ARG_WITH([confdir],[AS_HELP_STRING([--without-confdir],[do not install /etc/rrr.conf.d/])],[enable_confdir=no],[enable_confdir=yes])
AC_ARG_ENABLE([python3-debug],[AS_HELP_STRING([--enable-python3-debug],[build with Python 3 debugging])],[enable_python3_debug=yes],[enable_python3_debug=no])
AC_ARG_ENABLE([perl5-debug],[AS_HELP_STRING([--enable-perl5-debug],[build with Perl5 debugging])],[enable_perl5_debug=yes],[enable_perl5_debug=no])
AC_ARG_ENABLE([gnu-debug],[AS_HELP_STRING([--enable-gnu-debug],[build with debugging of RRR GNU functions; always enable compat functions])],[enable_gnu_debug=yes],[enable_gnu_debug=no])
AC_ARG_ENABLE([database-testing],[AS_HELP_STRING([--enable-database-testing],[enable checks requiring custom setup of databases])],[enable_database_testing=yes],[enable_database_testing=no])
AC_ARG_VAR([PYTHON3_FORCE_VERSION], [Force a specific python version to be used, e.g. 3.6])

AC_MSG_CHECKING([for RRR GNU Debug])
AS_IF([test "x$enable_gnu_debug" = xno], [
	AC_MSG_RESULT([RRR GNU Debug disabled])
], [
	AC_DEFINE([RRR_WITH_GNU_DEBUG], [1], [Enable RRR GNU Debug (compat functions always enabled)])
	AC_MSG_RESULT([yes])
])

CFLAGS_CODECOV=""
AC_MSG_CHECKING([for code coverage compilation])
AS_IF([test "x$enable_codecov" = xyes], [
	CFLAGS_CODECOV="-ftest-coverage -fprofile-arcs"
	AC_MSG_RESULT([$CFLAGS_CODECOV])
], [
	AC_MSG_RESULT([no])
])
AC_SUBST([CFLAGS_CODECOV])

AM_CFLAGS="$AM_CFLAGS $CFLAGS_CODECOV -std=c11 -include $srcdir/config.h -include $srcdir/build_timestamp.h -include $srcdir/src/lib/intercept.h -Wunreachable-code -Wstrict-prototypes -Wall -Wextra -Wno-clobbered -Werror=implicit-function-declaration -D_XOPEN_SOURCE=700"
AM_LDFLAGS=""

AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_LDFLAGS)

SHELL_VARS_INIT([variables.sh])

SHELL_VARS_EXPORT([RRR_WITH_SYSTEMD], $enable_systemd)
SHELL_VARS_EXPORT([RRR_WITH_PYTHON3], $enable_python3)
SHELL_VARS_EXPORT([RRR_WITH_PERL5], $enable_perl5)
SHELL_VARS_EXPORT([RRR_WITH_NGHTTP2], $enable_nghttp2)
SHELL_VARS_EXPORT([RRR_WITH_MYSQL], $enable_mysql)

SHELL_VARS_OUTPUT

AM_CONDITIONAL([RRR_WITH_OPENRC], [test "x$enable_openrc" != xno])
AM_CONDITIONAL([RRR_WITH_RUNIT], [test "x$enable_runit" != xno])
AM_CONDITIONAL([RRR_WITH_CONFDIR], [test "x$enable_confdir" != xno])
AM_CONDITIONAL([RRR_WITH_SYSTEMD], [test "x$enable_systemd" != xno])
AM_CONDITIONAL([RRR_WITH_USB], [test "x$enable_usb" != xno])
AM_CONDITIONAL([RRR_WITH_PYTHON3], [test "x$enable_python3" != xno])
AM_CONDITIONAL([RRR_WITH_PERL5], [test "x$enable_perl5" != xno])
AM_CONDITIONAL([RRR_WITH_JSONC], [test "x$enable_jsonc" != xno])
AM_CONDITIONAL([RRR_WITH_NGHTTP2], [test "x$enable_nghttp2" != xno])
AM_CONDITIONAL([RRR_WITH_MYSQL], [test "x$enable_mysql" != xno])

AC_CHECK_HEADER([linux/input.h], [have_linux_input=yes], [have_linux_input=no])
AM_CONDITIONAL([RRR_WITH_LINUX_INPUT], [test "x$have_linux_input" != xno])

AC_CHECK_HEADER([dev/evdev/input.h], [have_freebsd_input=yes], [have_freebsd_input=no])
AM_CONDITIONAL([RRR_WITH_FREEBSD_INPUT], [test "x$have_freebsd_input" != xno])

AC_MSG_CHECKING([for Linux-style input device headers])
AS_IF([test "x$have_linux_input" = xno], [
	AC_MSG_RESULT([no])
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([RRR_WITH_LINUX_INPUT], [1], [Enable Linux-style input device control])
])

AC_MSG_CHECKING([for FreeBSD-style input device headers])
AS_IF([test "x$have_freebsd_input" = xno], [
	AC_MSG_RESULT([no])
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([RRR_WITH_FREEBSD_INPUT], [1], [Enable FreeBSD-style input device control])
])

AC_MSG_CHECKING([location of limits.h with PATH_MAX])
AC_RUN_IFELSE([
	AC_LANG_SOURCE([[
		#include <linux/limits.h>
		#include <stdio.h>
		
		int main (int argc, char *argv[]) {
			printf("path_max: %i\n", PATH_MAX);
			return 0;
		}
	]])
], [
	AC_MSG_RESULT([linux/limits.h])
	AC_DEFINE([RRR_HAS_PATH_MAX_IN_LINUX_LIMITS_H], [1], [Use linux/limits.h to find PATH_MAX])
], [
	AC_MSG_RESULT([limits.h])
])

AC_MSG_CHECKING([precense of gettid()])
AC_RUN_IFELSE([
	AC_LANG_SOURCE([[
		#define _GNU_SOURCE
		#include <unistd.h>
		#include <sys/types.h>

		int main (int argc, char *argv[]) {
			pid_t tid = gettid();
			return 0;
		}
	]])
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([RRR_HAVE_GETTID], [1], [Linux-specific gettid() is present])
], [
	AC_MSG_RESULT([no])
])
		
AC_MSG_CHECKING([usage of eventfd()])
AS_IF([test "x$enable_eventfd" != "xno"], [
	AC_MSG_RESULT([yes])
	AC_MSG_CHECKING([precense of eventfd()])
	AC_RUN_IFELSE([
		AC_LANG_SOURCE([[
			#include <sys/eventfd.h>

			int main (int argc, char *argv[]) {
				int fd = eventfd(0, 0);
				return 0;
			}
		]])
	], [
		AC_MSG_RESULT([yes])
		AC_DEFINE([RRR_HAVE_EVENTFD], [1], [Linux-specific eventfd() is present])
	], [
		AC_MSG_RESULT([no])
	])
], [
	AC_MSG_RESULT([no])
])

AC_CHECK_HEADERS([event2/event.h event2/thread.h], [], [AC_MSG_ERROR([libevent headers not found])])
AC_CHECK_LIB([event], [event_base_new], [], [AC_MSG_ERROR([libevent library not found])])
AC_CHECK_LIB([event_pthreads], [evthread_use_pthreads], [], [AC_MSG_ERROR([libevent pthreads library not found])])

AC_SUBST([TLS_CFLAGS])
AC_SUBST([TLS_LDFLAGS])
AC_SUBST([TLS_LIBADD])

TLS_CFLAGS=
TLS_LDFLAGS=
TLS_LIBADD=

CHECK_LIBRESSL_LIBTLS

AC_MSG_CHECKING([for automatic LibreSSL/OpenSSL selection])
AS_IF([test "x$enable_auto_ssl" != xno], [
	AC_MSG_RESULT([yes])
	AS_IF([test "x$HAVE_LIBRESSL_LIBTLS" != "x"],[ enable_libressl=yes ])
	AC_CHECK_HEADERS([openssl/err.h], [ enable_openssl=yes ], [])
], [
	AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([for LibreSSL usage])
AS_IF([test "x$enable_libressl" = xyes], [
	AC_MSG_RESULT([yes])
	AS_IF([test "x$HAVE_LIBRESSL_LIBTLS" = "x"],[
		AC_MSG_ERROR([LibreSSL usage requested but libtls was not found])
	])
	AC_DEFINE([RRR_WITH_LIBRESSL], [1], [Enable encryption in network modules with LibreSSL])

	AC_MSG_CHECKING([LibreSSL CFLAGS])
	AC_MSG_RESULT([$LIBRESSL_LIBTLS_CFLAGS])

	AC_MSG_CHECKING([LibreSSL LDFLAGS])
	AC_MSG_RESULT([$LIBRESSL_LIBTLS_LDFLAGS])

	AC_MSG_CHECKING([LibreSSL LIBADD])
	AC_MSG_RESULT([$LIBRESSL_LIBTLS_LIBADD])

	TLS_CFLAGS=$LIBRESSL_LIBTLS_CFLAGS
	TLS_LDFLAGS=$LIBRESSL_LIBTLS_LDFLAGS
	TLS_LIBADD=$LIBRESSL_LIBTLS_LIBADD
], [
	AC_MSG_RESULT([Not compiling with LibreSSL])
])

AC_MSG_CHECKING([for LibreSSL overrides OpenSSL])
AS_IF([test "x$enable_auto_ssl" != "xno" && test "x$enable_openssl" = "xyes" && test "x$enable_libressl" = "xyes" ], [
	AC_MSG_RESULT([yes])
	enable_openssl=no
], [
	AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([for OpenSSL usage])
AS_IF([test "x$enable_openssl" = xyes], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([RRR_WITH_OPENSSL], [1], [Enable encryption in network modules with OpenSSL])
	AX_CHECK_OPENSSL([], [
		AC_MSG_ERROR([Cannot compile with encryption, OpenSSL missing])
	])
	AC_CHECK_HEADERS([openssl/crypto.h openssl/evp.h openssl/err.h openssl/sha.h openssl/rand.h])
# Only version >=1.1 supported
#	AC_CHECK_LIB(crypto, CRYPTO_get_new_dynlockid, [AC_DEFINE([RRR_HAVE_OLD_OPENSSL_LOCK], [1], [Have old style lock])], [])
	AC_CHECK_LIB(crypto, CRYPTO_THREAD_lock_new, [AC_DEFINE([RRR_HAVE_NEW_OPENSSL_LOCK], [1], [Have new style thread lock])], [])
	OPENSSL_CFLAGS_COMPAT="-DOPENSSL_API_COMPAT=0x10100000L"
	AC_SUBST([OPENSSL_CFLAGS_COMPAT])
	AC_DEFINE([OPENSSL_API_COMPAT], [0x10100000L], "Use OpenSSL version >= 1.1.0")

	AC_SUBST([TLS_CFLAGS])
	AC_SUBST([TLS_LDFLAGS])
	AC_SUBST([TLS_LIBADD])
], [
	AC_MSG_RESULT([Not compiling with OpenSSL])
])

AC_MSG_CHECKING([only one SSL/TLS library was chosen])
AS_IF([test "x$enable_openssl" = xyes && test "x$enable_libressl" = xyes ], [
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([cannot use both OpenSSL and LibreSSL at the same time, make sure auto-selection is not disabled])
], [
	AC_MSG_RESULT([yes])
])

AC_MSG_CHECKING([for LibreSSL/OpenSSL common cryptography functions])
AS_IF([test "x$enable_encryption" != "xno" ], [
	AC_MSG_RESULT([yes])
	AC_CHECK_HEADERS([openssl/crypto.h openssl/evp.h openssl/sha.h openssl/rand.h])
	AC_DEFINE([RRR_WITH_OPENSSL_CRYPT], [1], "Use OpenSSL/LibreSSL cryptography functions")
	AS_IF([test "x$TLS_LDFLAGS" = "x"], [
		AX_CHECK_OPENSSL([], [
			AC_MSG_ERROR([Cannot compile with encryption, OpenSSL/LibreSSL missing])
		])
		TLS_CFLAGS=$OPENSSL_INCLUDES
		TLS_LDFLAGS=$OPENSSL_LDFLAGS
		TLS_LIBADD=$OPENSSL_LIBS
	])
], [
	AC_MSG_RESULT([not enabled])
])

AC_SUBST([TLS_CFLAGS])
AC_SUBST([TLS_LDFLAGS])
AC_SUBST([TLS_LIBADD])

SHELL_VARS_EXPORT([RRR_WITH_OPENSSL], $enable_openssl)
SHELL_VARS_EXPORT([RRR_WITH_OPENSSL_CRYPT], $enable_openssl_crypt)
SHELL_VARS_EXPORT([RRR_WITH_LIBRESSL], $enable_libressl)
SHELL_VARS_EXPORT([RRR_WITH_AUTO_SSL], $enable_auto_ssl)

AM_CONDITIONAL([RRR_WITH_OPENSSL], [test "x$enable_openssl" != xno])
AM_CONDITIONAL([RRR_WITH_OPENSSL_CRYPT], [test "x$enable_encryption" != xno])
AM_CONDITIONAL([RRR_WITH_LIBRESSL], [test "x$enable_libressl" != xno])
AM_CONDITIONAL([RRR_WITH_AUTO_SSL], [test "x$enable_auto_ssl" != xno])

# Make sure Perl-test does not include any libraries
LIBS=""

AC_MSG_CHECKING([for perl5 enabled])
AS_IF([test "x$enable_perl5" = xno], [
	AC_MSG_RESULT([Not compiling with perl5])
], [
	AC_MSG_RESULT([yes])

	AC_DEFINE([RRR_WITH_PERL5], [1], [Enable perl5 extensions])

	if test x"${enable_perl5_debug}" = x"yes"; then
		AC_CHECK_PROG(PERL5_INTERPRETER,debugperl,debugperl)
	else
		AC_CHECK_PROG(PERL5_INTERPRETER,perl,perl)
	fi

	PERL5_REVISION=`perl -MConfig -e 'print "$Config{api_revision}\n"'`
	PERL5_VERSION=`perl -MConfig -e 'print "$Config{api_version}\n"'`
	PERL5_SUBVERSION=`perl -MConfig -e 'print "$Config{api_subversion}\n"'`

	AC_MSG_CHECKING([Perl5 >= 5.026])
	if test x"${PERL5_REVISION}" != "x5"; then
		AC_MSG_RESULT([no])
		AC_MSG_ERROR("Perl5 is required but not found")
	fi
	if test ${PERL5_VERSION} -lt 26; then
		AC_MSG_RESULT([no])
		AC_MSG_ERROR("Perl 5 version >= 26 is required but not found");
	fi
	AC_MSG_RESULT([yes])

	# Remove -specs=/... causing -pie to get inserted on RH-systems

	AC_MSG_CHECKING([Perl5 CFLAGS])
	PERL5_CFLAGS=`${PERL5_INTERPRETER} -MExtUtils::Embed -e ccopts | sed 's/\s*-specs\S*//g'`
	AC_MSG_RESULT([$PERL5_CFLAGS])

	AC_MSG_CHECKING([Perl5 LDFLAGS])
	perl5_ldflags_auto=`${PERL5_INTERPRETER} -MExtUtils::Embed -e ldopts | sed 's/\s*-specs\S*//g'`
	PERL5_LDFLAGS="$perl5_ldflags_auto"
	AC_MSG_RESULT([$PERL5_LDFLAGS])

	AC_MSG_CHECKING([perl5 compilation and linking])
	AC_LANG_CONFTEST([
		AC_LANG_SOURCE([[
			#include <EXTERN.h>
			#include <perl.h>
			int main (int argc, char **argv) {
				PERL_SYS_INIT3(&argc, &argv, NULL);
				PerlInterpreter *interpreter = perl_alloc();
				perl_free(interpreter);
				PERL_SYS_TERM();
				return 0;
			}
		]])
	])

	# Need -O to avoid warning about FORTIFY_SOURCE
	$CC -o conftest conftest.c ${PERL5_CFLAGS} -O3 ${PERL5_LDFLAGS}
	perl5_result=$?
	if test x"${perl5_result}" = x"0"; then
		./conftest
		perl5_result=$?
		if test x"${perl5_result}" = x"0"; then
			AC_MSG_RESULT([OK])
		else
			AC_MSG_ERROR([Test run of perl5 program failed with status "$?"])
		fi
	else
		AC_MSG_ERROR([Test of perl5 compilation failed, make sure libperl-dev is installed.])
	fi

	AC_MSG_CHECKING([Generating Perl5 XSI source])
	perl5_xsi_source="$srcdir/src/lib/perl5_xsi.c"
	${PERL5_INTERPRETER} -MExtUtils::Embed -e xsinit -- -o ${perl5_xsi_source}
	perl5_result=$?

	if test x"${perl5_result}" = x"0"; then
		AC_MSG_RESULT([$perl5_xsi_source])
	else
		AC_MSG_ERROR([Error while generating Perl5 XSI source file, result was "$perl5_result"])
	fi

	AC_SUBST([PERL5_CFLAGS])
	AC_SUBST([PERL5_LDFLAGS])

	AC_SUBST([PERL5_REVISION])
	AC_SUBST([PERL5_VERSION])
	AC_SUBST([PERL5_SUBVERSION])

	AC_SUBST([PERL5_INTERPRETER])
])

# Make sure Python-test does not include any libraries
LIBS=""

AC_MSG_CHECKING([for python3 enabled])
AS_IF([test "x$enable_python3" = xno], [
	AC_MSG_RESULT([Not compiling with python3])
], [
	AC_MSG_RESULT([yes])

	AC_DEFINE([RRR_WITH_PYTHON3], [1], [Enable python3 extensions])

	python3_version=""

	if test x"${PYTHON3_FORCE_VERSION}" != "x"; then
		python3="python${PYTHON3_FORCE_VERSION}"
		AC_CHECK_PROG(PYTHON3_INTERPRETER,"python${PYTHON3_FORCE_VERSION}",yes)
	else
		for version_tmp in 3.9 3.8 3.7; do
			python3_tmp="python$version_tmp"
			AC_CHECK_PROG(PYTHON3_INTERPRETER,${python3_tmp},yes)
			if test x"${PYTHON3_INTERPRETER}" = "xyes"; then
				python3_version=${version_tmp}
				break
			fi
		done
	fi

	python3="python${python3_version}"

	AC_MSG_CHECKING([python version >= 3.7 or forced version])
	if test "x${python3}" = "x"; then
		AC_MSG_ERROR("Supported version of python3 not found")
	fi
	AC_MSG_RESULT([${python3}])

	AC_MSG_CHECKING([for python3 debug enabled])
	if test "x$enable_python3_debug" = x"yes"; then
		python3_config_bin="${python3}-dbg-config"
		AC_MSG_RESULT([$python3_config_bin])
	else
		python3_config_bin="${python3}-config"
		AC_MSG_RESULT([no])
	fi

	python3_libs_from_pkg=""
	python3_cflags_from_pkg=""

	python3_embed_pkg_path="python-$python3_version-embed"

	python3_libs_from_pkg=`pkg-config $python3_embed_pkg_path --silence-errors --libs`
	python3_cflags_from_pkg=`pkg-config $python3_embed_pkg_path --silence-errors --cflags`

	AC_CHECK_PROG(PYTHON3_CONFIG,"$python3_config_bin",yes)
	if test x"${PYTHON3_CONFIG}" = x"yes"; then
		AC_DEFINE([RRR_WITH_PYTHON3], [1], [Build with python3 support], [])
	else
		AC_MSG_ERROR("Could not find python3-config for ${python3}")
	fi

	PYTHON3_CFLAGS=`$python3_config_bin --cflags | sed 's/-O3/ /g' | sed 's/-Wall/ /g'`
	PYTHON3_CFLAGS="${python3_cflags_from_pkg} ${PYTHON3_CFLAGS}"

	PYTHON3_LDFLAGS=`$python3_config_bin --ldflags`
	PYTHON3_LDFLAGS="${PYTHON3_LDFLAGS} $python3_libs_from_pkg"

	echo "Python3 CFLAGS: $PYTHON3_CFLAGS"
	echo "Python3 LDFLAGS: $PYTHON3_LDFLAGS"

	AC_SUBST([PYTHON3_CFLAGS])
	AC_SUBST([PYTHON3_LDFLAGS])

	chmod +x test_python3.sh
	./test_python3.sh "$python3_config_bin"
	PYTHON3_TEST_RESULT="$?"

	AC_MSG_CHECKING([functional python3 config])
	if test x"$PYTHON3_TEST_RESULT" = x"0"; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		echo -e "Include path is: "
		$python3_config_bin --includes
		AC_MSG_ERROR("Python3 test failed")
	fi
	
	AC_MSG_CHECKING([PyTypeObject tp_print member])
	cflags_orig="$CFLAGS"
	CFLAGS="$PYTHON3_CFLAGS"
	AC_RUN_IFELSE([
		AC_LANG_SOURCE([[
			#include <Python.h>
			
			int main (int argc, char *argv[]) {
				PyTypeObject o;
	    		o.tp_print = NULL;
				return 0;
			}
		]])
	], [
		AC_MSG_RESULT([yes])
		AC_DEFINE([RRR_PYTHON3_HAS_PTYPEOBJECT_TP_PRINT], [1], [Python3 PyTypeObject has .tp_print member (version <= 3.8)])
	], [
		AC_MSG_RESULT([no])
	])
	CFLAGS="$cflags_orig"
])

AC_MSG_CHECKING([for USB enabled])
AS_IF([test "x$enable_usb" = xno], [
	AC_MSG_RESULT([Not compiling with USB])
], [
	AC_MSG_RESULT([yes])
	AC_CHECK_HEADERS([usb.h])
	AC_CHECK_LIB(usb, usb_init, [], AC_MSG_ERROR([usb_init from libusb not found]))
	AC_CHECK_LIB(usb, usb_find_busses, [], AC_MSG_ERROR([usb_find_busses from libusb not found]))
	AC_CHECK_LIB(usb, usb_find_devices, [], AC_MSG_ERROR([usb_find_devices from libusb not found]))
	AC_MSG_CHECKING([usb.h symbols])
	AC_RUN_IFELSE([
		AC_LANG_SOURCE([[
			#include <usb.h>
			
			int main (int argc, char *argv[]) {
				struct usb_config_descriptor a;
				struct usb_device_descriptor b;
				struct usb_device c;
				struct usb_bus d;
				return 0;
			}				
		]])
	], [
		AC_MSG_RESULT([OK])
	], [
		AC_MSG_ERROR([Test of symbols from usb.h failed, make sure libusb-dev or libusb-compat is installed.])
	])
])

# Make sure json-c-test does not include any libraries
LIBS=""

AC_MSG_CHECKING([json-c bindings])
AS_IF([test "x$enable_jsonc" = xno], [
	AC_MSG_RESULT([Not compiling with json-c bindings])
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([RRR_WITH_JSONC], [1], [Compile with json-c bindings])
	AC_CHECK_HEADERS([json-c/json_tokener.h json-c/json_object.h json-c/json_object_iterator.h json-c/json_util.h])
	AC_CHECK_LIB(json-c, json_tokener_new, [], AC_MSG_ERROR([json_tokener_new from json-c not found]))

	AC_CHECK_LIB(json-c, json_object_new_null, [
		AC_DEFINE([RRR_HAVE_JSONC_NEW_NULL], [1], [We have json-c json_object_new_null])
	], [])

	AC_CHECK_LIB(json-c, json_object_new_uint64, [
		AC_DEFINE([RRR_HAVE_JSONC_NEW_UINT64], [1], [We have json-c json_object_new_uint64])
	], [])

	AC_MSG_CHECKING([json_object_object_add is of void type])
	AC_RUN_IFELSE([
		AC_LANG_SOURCE([[
			#include <json-c/json_object.h>
			int main (int argc, char *argv[]) {
				json_object *obj1 = json_object_new_object();
				json_object *obj2 = json_object_new_object();
				// Compilation fails if function is of void type
				return json_object_object_add(obj1, "key", obj2);
			}
		]])
	], [
		AC_MSG_RESULT([no])
	], [
		AC_MSG_RESULT([yes])
		AC_DEFINE([RRR_HAVE_JSONC_OBJECT_ADD_VOID], [1], [json_object_object_add is of void type])
	])

	JSONC_CFLAGS=""
	JSONC_LDFLAGS=""
	JSONC_LIBS="-ljson-c"

	AC_SUBST([JSONC_CFLAGS])
	AC_SUBST([JSONC_LDFLAGS])
	AC_SUBST([JSONC_LIBS])
])

AC_MSG_CHECKING([NGHTTP2 bindings])
AS_IF([test "x$enable_nghttp2" = xno], [
	AC_MSG_RESULT([Not compiling with NGHTTP2 bindings])
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([RRR_WITH_NGHTTP2], [1], [Compile with NGHTTP2 bindings])
	AC_CHECK_HEADERS([nghttp2/nghttp2.h])
	AC_CHECK_LIB(nghttp2, nghttp2_pack_settings_payload, [], AC_MSG_ERROR([nghttp2_pack_settings_payload from nghttp2 not found]))
	NGHTTP2_CFLAGS=""
	NGHTTP2_LDFLAGS="-lnghttp2"

	AC_SUBST([NGHTTP2_CFLAGS])
	AC_SUBST([NGHTTP2_LDFLAGS])
	AC_SUBST([NGHTTP2_LIBS])
])

# Make sure MySQL-test does not include any libraries
LIBS=""

AC_MSG_CHECKING([MySQL module inclusion])
AS_IF([test "x$enable_mysql" = xno], [
	AC_MSG_RESULT([Not compiling with MySQL module])
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([RRR_WITH_MYSQL], [1], [Compile with MySQL module])
	AX_LIB_MYSQL([5.0.0])
	AC_MSG_CHECKING([headers for MariaDB/MySQL])
	AC_RUN_IFELSE([
		AC_LANG_SOURCE([[
			#include <mysql/mysql.h>
			
			int main (int argc, char *argv[]) {
				return 0;
			}
		]])
	], [
		AC_MSG_RESULT([OK])
	], [
		AC_MSG_ERROR([MariaDB/MySQL header mysql/mysql.h not found])
	])
	AC_MSG_CHECKING([integer sizes for MariaDB/MySQL])
	AC_RUN_IFELSE([
		AC_LANG_SOURCE([[
			#include <mysql/mysql.h>
			#include <stdint.h>
			#include <stdio.h>
			
			int main (int argc, char *argv[]) {
				FILE *fd = fopen("conftest.out", "w+");
				if (fd == NULL) {
					return 1;
				}
			
				int ret = 0;
				if (sizeof(unsigned long long int) < sizeof(uint64_t)) {
					fprintf (fd, "Error: Size of 'unsigned long long int' is too small to fit 'uint64_t'\n");
					ret = 1;
				}
				if (sizeof(unsigned long int) < sizeof(uint32_t)) {
					fprintf (fd, "Error: Size of 'unsigned long int' is too small to fit 'uint32_t'\n");
					ret = 1;
				}
				fclose(fd);
			
				return ret;
			}
		]])
	], [
		AC_MSG_RESULT([OK])
	], [
		echo "Test returned: `cat conftest.out`";
		AC_MSG_ERROR([Size test of integers failed, cannot compile with MySQL on this system.])
	])
])

AC_MSG_CHECKING([enabled database testing])
AS_IF([test "x$enable_database_testing" = xyes], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([RRR_ENABLE_DB_TESTING], [1], [Enable database testing in test suite])
],[
	AC_MSG_RESULT([no])
])

AC_DEFUN([REQUIRE_CONFDIR], [
	if [test "x$enable_confdir" != "xyes"]; then
		AC_MSG_ERROR([confdir was not enabled, but is required])
	fi
])

AC_MSG_CHECKING([for RRR environment file directory])
RRR_ENVIRONMENT_FILE_DIR="${sysconfdir}"
AC_MSG_RESULT([${sysconfdir}])
AC_SUBST(RRR_ENVIRONMENT_FILE_DIR)

AC_MSG_CHECKING([for RRR config file directory])
RRR_CONF_DIR="${sysconfdir}/rrr.conf.d"
AC_MSG_RESULT([${sysconfdir}/rrr.conf.d])
AC_SUBST(RRR_CONF_DIR)

AC_MSG_CHECKING([for confdir usage])
AS_IF([test "x$enable_confdir" = xyes], [
	AC_CONFIG_FILES([misc/confdir/Makefile])

	AC_MSG_RESULT([yes, ${RRR_CONF_DIR}])
],[
	AC_MSG_RESULT([no])
])

AM_CONDITIONAL([RRR_SYSTEMD_NO_UNIT_INSTALL], [test "x$RRR_SYSTEMD_NO_UNIT_INSTALL" = "x1"])
AM_CONDITIONAL([RRR_SYSTEMD_NO_TMPFILES_INSTALL], [test "x$RRR_SYSTEMD_NO_TMPFILES_INSTALL" = "x1"])

AC_MSG_CHECKING([for systemd usage])
AS_IF([test "x$enable_systemd" = xyes], [
	REQUIRE_CONFDIR
	SYSTEMD_INIT

	AC_MSG_RESULT([yes]);

	AC_SUBST([RRR_SYSTEMD_UNIT_DIR], [SYSTEMD_SYSTEM_UNIT_DIR])
	AC_MSG_CHECKING([For systemd unit directory])
	AC_MSG_RESULT([SYSTEMD_SYSTEM_UNIT_DIR])

	AC_SUBST([RRR_TMPFILES_DIR], [SYSTEMD_TMPFILES_DIR])
	AC_MSG_CHECKING([For systemd tmpfiles.d directory])
	AC_MSG_RESULT([SYSTEMD_TMPFILES_DIR])

	AC_CONFIG_FILES([misc/systemd/make-service.sh])
	AC_CONFIG_FILES([misc/systemd/make-tmpfiles.sh])
	AC_CONFIG_FILES([misc/systemd/Makefile])

	AC_MSG_CHECKING([for systemd service file no installation command])
	AS_IF([test "x$RRR_SYSTEMD_NO_INSTALL" = "x1"] , [
		AC_MSG_RESULT([yes])
	], [
		AC_MSG_RESULT([no])
	])

	AC_CHECK_HEADER([systemd/sd-journal.h], [], AC_MSG_ERROR([missing SystemD development headers]))
	AC_CHECK_LIB(systemd, sd_journal_print, [], AC_MSG_ERROR([sd_journal_print from libsystemd]))

	AC_DEFINE([HAVE_JOURNALD], [1], [Have native SystemD/journald logging])

	JOURNALD_LIBS="-lsystemd"
	AC_SUBST([JOURNALD_LIBS])

	RRR_RUN_DIR="/run/rrr"
],[
	RRR_RUN_DIR="/tmp"
	AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([for RRR run directory])
AC_MSG_RESULT([${RRR_RUN_DIR}])
AC_SUBST(RRR_RUN_DIR)
AC_DEFINE_UNQUOTED([RRR_RUN_DIR], ["${RRR_RUN_DIR}"], [Default run directory for sockets])

AC_MSG_CHECKING([for RRR data directory])
RRR_DATA_DIR="${localstatedir}/lib/rrr"
AC_MSG_RESULT([${RRR_DATA_DIR}])
AC_SUBST(RRR_DATA_DIR)

AC_CONFIG_FILES([src/make-paths.sh])

AC_MSG_CHECKING([for daemon username])
AS_IF([test "x$RRR_DAEMON_USERNAME" != "x"] , [
	AC_MSG_RESULT([${RRR_DAEMON_USERNAME}])
], [
	RRR_DAEMON_USERNAME=daemon
	AC_MSG_RESULT([${RRR_DAEMON_USERNAME} (by default)])
])
AC_SUBST(RRR_DAEMON_USERNAME)

AC_MSG_CHECKING([for runit usage])
AS_IF([test "x$enable_runit" = xyes], [
	REQUIRE_CONFDIR
	AC_SUBST([RUNIT_SV_DIR], [/etc/sv/rrr/])
	AC_CONFIG_FILES([misc/runit/Makefile])
	
	AC_CONFIG_FILES([misc/runit/make-runit.sh])
	AC_MSG_RESULT([yes, sv directory is ${RUNIT_SV_DIR}])
],[
	AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([for openrc usage])
AS_IF([test "x$enable_openrc" = xyes], [
	REQUIRE_CONFDIR
	AC_CONFIG_FILES([misc/openrc/Makefile])
	AC_CONFIG_FILES([misc/openrc/make-service.sh])
	AC_MSG_RESULT([yes])
],[
	AC_MSG_RESULT([no])
])

MODULE_DIR="\${exec_prefix}/lib/rrr"
AC_MSG_CHECKING([module installation directory])
AC_MSG_RESULT([${MODULE_DIR}])

CMODULE_DIR="\${exec_prefix}/lib/rrr/cmodules"
AC_MSG_CHECKING([cmodule installation directory])
AC_MSG_RESULT([${CMODULE_DIR}])

AC_SUBST([MODULE_DIR])
AC_SUBST([CMODULE_DIR])

# Clear all automatically added libs. Library users must specify
# libraries locally in Makefile.am
LIBS=""

AM_COND_IF([RRR_WITH_PERL5], [AC_CONFIG_FILES([src/perl5/xsub/Makefile.PL])])
AM_COND_IF([RRR_WITH_PYTHON3], [AC_CONFIG_FILES([src/python/Makefile])])

AC_CONFIG_FILES([src/tests/test.sh],[chmod +x src/tests/test.sh])
AC_CONFIG_FILES([src/test.sh],[chmod +x src/test.sh])

AC_CONFIG_FILES(
	Makefile
	src/Makefile
	src/modules/Makefile
	src/lib/Makefile
	src/tests/Makefile
	src/tests/modules/Makefile
	misc/Makefile
	src/cmodules/Makefile
)

AC_OUTPUT
SHELL_VARS_CLEANUP

# Make sure dependency_libs is always set to empty in new .la files to avoid
# recursive depedency linking
echo "rrr: disabling libtool depedency_libs variable"
sed "s/^dependency_libs=.*/dependency_libs=''/" < libtool > libtool.new
mv libtool.new libtool
chmod +x libtool

# Copy config.h file for installation together with other headers
cp -f config.h src/lib/rrr-build-config.h

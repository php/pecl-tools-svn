dnl $Id$
dnl config.m4 for extension svn
dnl vim:se ts=2 sw=2 et:

PHP_ARG_WITH(svn, for svn support,

[  --with-svn=[/path/to/svn-prefix]      Include svn support])

PHP_ARG_WITH(svn-apr, for specifying the location of apr for svn,

[  --with-svn-apr=[/path/to/apr-prefix]  Location of apr-1-config / apr-config])


if test "$PHP_SVN" != "no"; then

	AC_MSG_CHECKING([for svn includes])
	for i in $PHP_SVN /usr/local /usr /opt /sw; do
		if test -r $i/include/subversion-1/svn_client.h ; then
			SVN_DIR=$i
			PHP_SVN_INCLUDES="-I$i/include/subversion-1"
			PHP_SVN_LDFLAGS="-lsvn_client-1 -lsvn_fs-1 -lsvn_repos-1 -lsvn_subr-1"
			break;
		fi
	done

	if test "$PHP_SVN_LDFLAGS" == ""; then
		AC_MSG_ERROR([failed to find svn_client.h])
	fi

	dnl check SVN version, we need at least 1.4

	for i in $PHP_SVN_APR $PHP_SVN /usr/local /usr /opt /sw; do
		dnl APR 1.0 tests
		if test -r $i/bin/apr-1-config ; then
			apr_config_path="$i/bin/apr-1-config"
			break;
		elif test -r $i/apache2/bin/apr-1-config ; then
			apr_config_path="$i/apache2/bin/apr-1-config"
			break;
		elif test -r $i/apache/bin/apr-1-config ; then
			apr_config_path="$i/apache/bin/apr-1-config"
			break;
		dnl APR 0.9 tests
		elif test -r $i/bin/apr-config ; then
			apr_config_path="$i/bin/apr-config"
			break;
		elif test -r $i/apache2/bin/apr-config ; then
			apr_config_path="$i/apache2/bin/apr-config"
			break;
		elif test -r $i/apache/bin/apr-config ; then
			apr_config_path="$i/apache/bin/apr-config"
			break;
		fi
	done
	if test "$apr_config_path" == ""; then
		AC_MSG_ERROR([failed to find apr-config / apr-1-config])
	fi

	APR_INCLUDES=`$apr_config_path --includes --cppflags`
	APR_LDFLAGS=`$apr_config_path --link-ld`

	PHP_SVN_INCLUDES="$PHP_SVN_INCLUDES $APR_INCLUDES"
	PHP_SVN_LDFLAGS="$PHP_SVN_LDFLAGS $APR_LDFLAGS"

	AC_MSG_RESULT($PHP_SVN_INCLUDES)
	AC_MSG_CHECKING([for svn libraries])
	AC_MSG_RESULT($PHP_SVN_LDFLAGS)

	PHP_CHECK_LIBRARY(svn_client-1, svn_client_move3,[
		SVN_13="yes"
	],
	,-L$SVN_DIR/$PHP_LIBDIR)

	if test "$SVN_13" == ""; then
		AC_MSG_ERROR([failed to determine svn version or less than 1.3])
	fi

	AC_DEFINE(HAVE_SVNLIB,1,[ ])

	INCLUDES="$INCLUDES $PHP_SVN_INCLUDES"

	PHP_EVAL_LIBLINE($PHP_SVN_LDFLAGS, SVN_SHARED_LIBADD)
	PHP_SUBST(SVN_SHARED_LIBADD)

	PHP_NEW_EXTENSION(svn, svn.c, $ext_shared,,$PHP_SVN_INCLUDES)
fi

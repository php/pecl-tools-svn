dnl $Id$
dnl config.m4 for extension svn
dnl vim:se ts=2 sw=2 et:

PHP_ARG_WITH(svn, for svn support,

[  --with-svn=[/path/to/svn-config]      Include svn support])


if test "$PHP_SVN" != "no"; then
  AC_MSG_CHECKING([for svn-config])
  if ! test -x $PHP_SVN ; then
    for i in $PHP_SVN /usr/local/bin /usr/bin ; do
      if test -x $i/svn-config ; then
        PHP_SVN=$i/svn-config
        break;
      fi
    done
  fi
  if test -x $PHP_SVN ; then
    AC_MSG_RESULT($PHP_SVN)
    PHP_SVN_INCLUDES=`$PHP_SVN --includes`
    PHP_SVN_CFLAGS=`$PHP_SVN --cppflags --cflags`
    PHP_SVN_LDFLAGS=`$PHP_SVN --ldflags --libs`
  else
    AC_MSG_RESULT([not found])
    AC_MSG_WARN([
Did not find svn-config; please ensure that you have installed
the svn developer package or equivalent for you system.
])
    PHP_SVN_INCLUDES=""
    for i in $PHP_SVN /usr/local/bin /usr/bin /usr ; do
      if test -r $i/include/subversion-1/svn_client.h ; then
        PHP_SVN_INCLUDES="$PHP_SVN_INCLUDES -I$i/include/subversion-1"
        PHP_SVN_LDFLAGS="-L$i/lib"
        if test -d $i/lib64 ; then
          PHP_SVN_LDFLAGS="-L$i/lib64 $PHP_SVN_LDFLAGS"
        fi
        PHP_SVN_LDFLAGS="$PHP_SVN_LDFLAGS -lsvn_client-1 -lapr-0"
      fi
      if test -r $i/include/apr-0/apr.h ; then
        PHP_SVN_INCLUDES="$PHP_SVN_INCLUDES -I$i/include/apr-0"
      elif test -r $i/include/apache2/apr.h ; then
        PHP_SVN_INCLUDES="$PHP_SVN_INCLUDES -I$i/include/apache2"
      fi
    done
  fi

  PHP_CHECK_LIBRARY(svn_client-1,svn_client_create_context,
  [
    PHP_ADD_LIBRARY(svn_client-1, 1, SVN_SHARED_LIBADD)
    AC_DEFINE(HAVE_SVNLIB,1,[ ])
  ],[
    AC_MSG_ERROR([wrong svn lib version or lib not found])
  ],[
    $PHP_SVN_LDFLAGS
  ])
  
  PHP_EVAL_LIBLINE($PHP_SVN_LDFLAGS, SVN_SHARED_LIBADD)
  PHP_SUBST(SVN_SHARED_LIBADD)

  PHP_NEW_EXTENSION(svn, svn.c, $ext_shared,,$PHP_SVN_INCLUDES $PHP_SVN_CFLAGS)
fi

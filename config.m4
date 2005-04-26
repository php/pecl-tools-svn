dnl $Id$
dnl config.m4 for extension svn
dnl vim:se ts=2 sw=2 et:

PHP_ARG_WITH(svn, for svn support,

[  --with-svn             Include svn support])


if test "$PHP_SVN" != "no"; then
  # --with-svn -> check with-path
  SEARCH_PATH="/usr/local /usr"     # you might want to change this
  SEARCH_FOR="/include/subversion-1/svn_client.h"  # you most likely want to change this
  if test -r $PHP_SVN/$SEARCH_FOR; then # path given as parameter
    SVN_DIR=$PHP_SVN
  else # search default path list
    AC_MSG_CHECKING([for svn files in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        SVN_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi
  
  if test -z "$SVN_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the svn distribution])
  fi

  # --with-svn -> add include path
  PHP_ADD_INCLUDE($SVN_DIR/include/subversion-1)
  PHP_ADD_INCLUDE($SVN_DIR/include/apr-0)

  # --with-svn -> check for lib and symbol presence
  LIBNAME=svn_client-1
  LIBSYMBOL=svn_client_create_context

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $SVN_DIR/lib, SVN_SHARED_LIBADD)
    AC_DEFINE(HAVE_SVNLIB,1,[ ])
  ],[
    AC_MSG_ERROR([wrong svn lib version or lib not found])
  ],[
    -L$SVN_DIR/lib -lm -ldl -lsvn_client-1 -lapr-0
  ])
  
  PHP_SUBST(SVN_SHARED_LIBADD)

  PHP_NEW_EXTENSION(svn, svn.c, $ext_shared)
fi

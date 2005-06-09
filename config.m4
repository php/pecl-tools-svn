dnl $Id$
dnl config.m4 for extension svn
dnl vim:se ts=2 sw=2 et:

PHP_ARG_WITH(svn, for svn support,

[  --with-svn=[/path/to/svn-prefix]      Include svn support])

PHP_ARG_WITH(svn-apr, for specifying the location of apr for svn,

[  --with-svn-apr=[/path/to/apr-prefix]  Location of /include/*/apr.h])


if test "$PHP_SVN" != "no"; then
  AC_MSG_CHECKING([for svn includes])
  for i in $PHP_SVN /usr/local /usr /opt /sw; do
    if test -r $i/include/subversion-1/svn_client.h ; then
      PHP_SVN_INCLUDES="-I$i/include/subversion-1"
      PHP_SVN_LDFLAGS="-L$i/lib"
      if test -d $i/lib64 ; then
        PHP_SVN_LDFLAGS="-L$i/lib64"
      fi
      PHP_SVN_LDFLAGS="-lsvn_client-1 -lapr-0"
      break;
    fi
  done
  if test "$PHP_SVN_LDFLAGS" == ""; then
     AC_MSG_ERROR([failed to find svn_client.h])
  fi
  
  
  for i in $PHP_SVN_APR $PHP_SVN /usr/local /usr /opt /sw; do 
    if test -r $i/include/apr-0/apr.h ; then
      PHP_SVN_INCLUDES="$PHP_SVN_INCLUDES -I$i/include/apr-0"
      PHP_SVN_LDFLAGS="$PHP_SVN_LDFLAGS -L$i/lib"
      PHP_SVN_APR_FOUND="yes"
      break;
    elif test -r $i/include/apache2/apr.h ; then
      PHP_SVN_INCLUDES="$PHP_SVN_INCLUDES -I$i/include/apache2"
      PHP_SVN_LDFLAGS="$PHP_SVN_LDFLAGS -L$i/lib"
      PHP_SVN_APR_FOUND="yes"
      break;
    fi
  done
  if test "$PHP_SVN_APR_FOUND" == ""; then
     AC_MSG_ERROR([failed to find apr.h])
  fi

  AC_MSG_RESULT($PHP_SVN_INCLUDES)
  AC_MSG_CHECKING([for svn libraries])
  AC_MSG_RESULT($PHP_SVN_LDFLAGS)

  AC_DEFINE(HAVE_SVNLIB,1,[ ])
  
  INCLUDES="$INCLUDES $PHP_SVN_INCLUDES"
  
  PHP_EVAL_LIBLINE($PHP_SVN_LDFLAGS, SVN_SHARED_LIBADD)
  PHP_SUBST(SVN_SHARED_LIBADD)

  PHP_NEW_EXTENSION(svn, svn.c, $ext_shared,,$PHP_SVN_INCLUDES)
fi

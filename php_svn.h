/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2005 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Alan Knowles <alan@akbkhome.com>                            |
  |          Wez Furlong <wez@omniti.com>                                |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_SVN_H
#define PHP_SVN_H

extern zend_module_entry svn_module_entry;
#define phpext_svn_ptr &svn_module_entry

#ifdef PHP_WIN32
#define PHP_SVN_API __declspec(dllexport)
#else
#define PHP_SVN_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif
#include "svn_client.h"

PHP_MINIT_FUNCTION(svn);
PHP_MSHUTDOWN_FUNCTION(svn);
PHP_RINIT_FUNCTION(svn);
PHP_RSHUTDOWN_FUNCTION(svn);
PHP_MINFO_FUNCTION(svn);

PHP_FUNCTION(svn_checkout);
PHP_FUNCTION(svn_cat);
PHP_FUNCTION(svn_ls);
PHP_FUNCTION(svn_log);
PHP_FUNCTION(svn_auth_set_parameter);
PHP_FUNCTION(svn_auth_get_parameter);
PHP_FUNCTION(svn_client_version);
PHP_FUNCTION(svn_diff);
PHP_FUNCTION(svn_cleanup);

PHP_FUNCTION(svn_commit);
PHP_FUNCTION(svn_add);
PHP_FUNCTION(svn_status);
PHP_FUNCTION(svn_update);

PHP_FUNCTION(svn_repos_create);
PHP_FUNCTION(svn_repos_recover);
PHP_FUNCTION(svn_repos_hotcopy);

/* TODO: */

PHP_FUNCTION(svn_blame);
PHP_FUNCTION(svn_merge);
PHP_FUNCTION(svn_revert);
PHP_FUNCTION(svn_resolved);
PHP_FUNCTION(svn_copy);
PHP_FUNCTION(svn_move);
PHP_FUNCTION(svn_propset);
PHP_FUNCTION(svn_propget);
PHP_FUNCTION(svn_proplist);
PHP_FUNCTION(svn_export);
PHP_FUNCTION(svn_url_from_path);
PHP_FUNCTION(svn_uuid_from_url);
PHP_FUNCTION(svn_uuid_from_path);
PHP_FUNCTION(svn_switch);
PHP_FUNCTION(svn_mkdir);
PHP_FUNCTION(svn_delete);
PHP_FUNCTION(svn_import);


ZEND_BEGIN_MODULE_GLOBALS(svn)
	apr_pool_t *pool;
	svn_client_ctx_t *ctx;
ZEND_END_MODULE_GLOBALS(svn)

#ifdef ZTS
#define SVN_G(v) TSRMG(svn_globals_id, zend_svn_globals *, v)
#else
#define SVN_G(v) (svn_globals.v)
#endif

#endif	/* PHP_SVN_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

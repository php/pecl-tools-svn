/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2008 The PHP Group                                |
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
  |          Luca Furini <lfurini@cs.unibo.it>                           |
  |          Jerome Renard <jerome.renard_at_gmail.com>                  |
  |          Develar <develar_at_gmail.com>                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_svn.h"

#include "svn_pools.h"
#include "svn_sorts.h"
#include "svn_config.h"
#include "svn_auth.h"
#include "svn_path.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_utf.h"
#include "svn_time.h"

/* If you declare any globals in php_svn.h uncomment this: */
ZEND_DECLARE_MODULE_GLOBALS(svn)

/* custom property for ignoring SSL cert verification errors */
#define PHP_SVN_AUTH_PARAM_IGNORE_SSL_VERIFY_ERRORS "php:svn:auth:ignore-ssl-verify-errors"
#define PHP_SVN_INIT_CLIENT() \
	if (init_svn_client(TSRMLS_C)) {\
		RETURN_FALSE;\
	}


static void php_svn_get_version(char *buf, int buflen);

/* True global resources - no need for thread safety here */

struct php_svn_repos {
	long rsrc_id;
	apr_pool_t *pool;
	svn_repos_t *repos;
};

struct php_svn_fs {
	struct php_svn_repos *repos;
	svn_fs_t *fs;
};

struct php_svn_fs_root {
	struct php_svn_repos *repos;
	svn_fs_root_t *root;
};

struct php_svn_repos_fs_txn {
	struct php_svn_repos *repos;
	svn_fs_txn_t *txn;
};


struct php_svn_log_receiver_baton {
	zval *result;
	svn_boolean_t omit_messages;
};
/* class entry constants */
static zend_class_entry *ce_Svn;


/* resource constants */
static int le_svn_repos;
static int le_svn_fs;
static int le_svn_fs_root;
static int le_svn_repos_fs_txn;

static ZEND_RSRC_DTOR_FUNC(php_svn_repos_dtor)
{
	struct php_svn_repos *r = rsrc->ptr;
	svn_pool_destroy(r->pool);
	efree(r);
}

static ZEND_RSRC_DTOR_FUNC(php_svn_fs_dtor)
{
	struct php_svn_fs *r = rsrc->ptr;
	zend_list_delete(r->repos->rsrc_id);
	efree(r);
}

static ZEND_RSRC_DTOR_FUNC(php_svn_fs_root_dtor)
{
	struct php_svn_fs_root *r = rsrc->ptr;
	zend_list_delete(r->repos->rsrc_id);
	efree(r);
}

static ZEND_RSRC_DTOR_FUNC(php_svn_repos_fs_txn_dtor)
{
	struct php_svn_repos_fs_txn *r = rsrc->ptr;
	zend_list_delete(r->repos->rsrc_id);
	efree(r);
}

#define SVN_STATIC_ME(name) ZEND_FENTRY(name, ZEND_FN(svn_ ## name), NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
/** Fixme = this list needs padding out... */
static function_entry svn_methods[] = {
	SVN_STATIC_ME(cat)
	SVN_STATIC_ME(checkout)
	SVN_STATIC_ME(log)
	SVN_STATIC_ME(status)

	{NULL, NULL, NULL}
};




/* {{{ svn_functions[] */
function_entry svn_functions[] = {
	PHP_FE(svn_checkout,		NULL)
	PHP_FE(svn_cat,			NULL)
	PHP_FE(svn_ls,			NULL)
	PHP_FE(svn_log,			NULL)
	PHP_FE(svn_auth_set_parameter,	NULL)
	PHP_FE(svn_auth_get_parameter,	NULL)
	PHP_FE(svn_client_version, NULL)
	PHP_FE(svn_diff, NULL)
	PHP_FE(svn_cleanup, NULL)
	PHP_FE(svn_revert, NULL)
	PHP_FE(svn_resolved, NULL)
	PHP_FE(svn_commit, NULL)
	PHP_FE(svn_add, NULL)
	PHP_FE(svn_status, NULL)
	PHP_FE(svn_update, NULL)
	PHP_FE(svn_import, NULL)
	PHP_FE(svn_info, NULL)
	PHP_FE(svn_export, NULL)
	PHP_FE(svn_copy, NULL)
	PHP_FE(svn_switch, NULL)
	PHP_FE(svn_blame, NULL)
	PHP_FE(svn_delete, NULL)
	PHP_FE(svn_mkdir, NULL)
	PHP_FE(svn_move, NULL)
	PHP_FE(svn_proplist, NULL)
	PHP_FE(svn_propget, NULL)
	PHP_FE(svn_repos_create, NULL)
	PHP_FE(svn_repos_recover, NULL)
	PHP_FE(svn_repos_hotcopy, NULL)
	PHP_FE(svn_repos_open, NULL)
	PHP_FE(svn_repos_fs, NULL)
	PHP_FE(svn_repos_fs_begin_txn_for_commit, NULL)
	PHP_FE(svn_repos_fs_commit_txn, NULL)
	PHP_FE(svn_fs_revision_root, NULL)
	PHP_FE(svn_fs_check_path, NULL)
	PHP_FE(svn_fs_revision_prop, NULL)
	PHP_FE(svn_fs_dir_entries, NULL)
	PHP_FE(svn_fs_node_created_rev, NULL)
	PHP_FE(svn_fs_youngest_rev, NULL)
	PHP_FE(svn_fs_file_contents, NULL)
	PHP_FE(svn_fs_file_length, NULL)
	PHP_FE(svn_fs_txn_root, NULL)
	PHP_FE(svn_fs_make_file, NULL)
	PHP_FE(svn_fs_make_dir, NULL)
	PHP_FE(svn_fs_apply_text, NULL)
	PHP_FE(svn_fs_copy, NULL)
	PHP_FE(svn_fs_delete, NULL)
	PHP_FE(svn_fs_begin_txn2, NULL)
	PHP_FE(svn_fs_is_dir, NULL)
	PHP_FE(svn_fs_is_file, NULL)
	PHP_FE(svn_fs_node_prop, NULL)
	PHP_FE(svn_fs_change_node_prop, NULL)
	PHP_FE(svn_fs_contents_changed, NULL)
	PHP_FE(svn_fs_props_changed, NULL)
	PHP_FE(svn_fs_abort_txn, NULL)

	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ svn_module_entry */
zend_module_entry svn_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"svn",
	svn_functions,
	PHP_MINIT(svn),
	NULL,
	NULL,
	PHP_RSHUTDOWN(svn),
	PHP_MINFO(svn),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_SVN_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */


#ifdef COMPILE_DL_SVN
ZEND_GET_MODULE(svn)
#endif

/* {{{ php_svn_get_revision_kind */
static enum svn_opt_revision_kind php_svn_get_revision_kind(svn_opt_revision_t revision)
{
	switch(revision.value.number) {
 		case svn_opt_revision_unspecified:
 			/* through  */
 		case SVN_REVISION_HEAD:
 			return svn_opt_revision_head;
 		case SVN_REVISION_BASE:
 			return svn_opt_revision_base;
 		case SVN_REVISION_COMMITTED:
 			return svn_opt_revision_committed;
 		case SVN_REVISION_PREV:
 			return svn_opt_revision_previous;
 		default:
 			return svn_opt_revision_number;
	}
}
/* }}} */


#include "ext/standard/php_smart_str.h"
static void php_svn_handle_error(svn_error_t *error TSRMLS_DC)
{
	svn_error_t *itr = error;
	smart_str s = {0,0,0};

	smart_str_appendl(&s, "svn error(s) occured\n", sizeof("svn error(s) occured\n")-1);

	while (itr) {
		char buf[256];

		smart_str_append_long(&s, itr->apr_err);
		smart_str_appendl(&s, " (", 2);

		svn_strerror(itr->apr_err, buf, sizeof(buf));
		smart_str_appendl(&s, buf, strlen(buf));
		smart_str_appendl(&s, ") ", 2);
		smart_str_appendl(&s, itr->message, strlen(itr->message));

		if (itr->child) {
			smart_str_appendl(&s, "\n", 1);
		}
		itr = itr->child;
	}

	smart_str_appendl(&s, "\n", 1);
	smart_str_0(&s);
	php_error_docref(NULL TSRMLS_CC, E_WARNING, s.c);
	smart_str_free(&s);
}

static svn_error_t *php_svn_auth_ssl_client_server_trust_prompter(
	svn_auth_cred_ssl_server_trust_t **cred,
	void *baton,
	const char *realm,
	apr_uint32_t failures,
	const svn_auth_ssl_server_cert_info_t *cert_info,
	svn_boolean_t may_save,
	apr_pool_t *pool)
{
	const char *ignore;
	TSRMLS_FETCH();

	ignore = (const char*)svn_auth_get_parameter(SVN_G(ctx)->auth_baton, PHP_SVN_AUTH_PARAM_IGNORE_SSL_VERIFY_ERRORS);
	if (ignore && atoi(ignore)) {
		*cred = apr_palloc(SVN_G(pool), sizeof(**cred));
		(*cred)->may_save = 0;
		(*cred)->accepted_failures = failures;
	}

	return SVN_NO_ERROR;
}

static void php_svn_init_globals(zend_svn_globals *g)
{
	memset(g, 0, sizeof(*g));
}

static svn_error_t *php_svn_get_commit_log(const char **log_msg, const char **tmp_file,
		apr_array_header_t *commit_items, void *baton, apr_pool_t *pool)
{
	*log_msg = (const char*)baton;
	*tmp_file = NULL;
	return SVN_NO_ERROR;
}

static int init_svn_client(TSRMLS_D)
{
	svn_error_t *err;
	svn_auth_provider_object_t *provider;
	svn_auth_baton_t *ab;
	apr_array_header_t *providers;

	if (SVN_G(pool)) return 0;

	SVN_G(pool) = svn_pool_create(NULL);

	if ((err = svn_client_create_context (&SVN_G(ctx), SVN_G(pool)))) {
		php_svn_handle_error(err TSRMLS_CC);
		svn_pool_destroy(SVN_G(pool));
		SVN_G(pool) = NULL;
		return 1;
	}

	if ((err = svn_config_get_config(&SVN_G(ctx)->config, NULL, SVN_G(pool)))) {
		php_svn_handle_error(err TSRMLS_CC);
		svn_pool_destroy(SVN_G(pool));
		SVN_G(pool) = NULL;
		return 1;
	}

	SVN_G(ctx)->log_msg_func = php_svn_get_commit_log;

	/* The whole list of registered providers */

	providers = apr_array_make (SVN_G(pool), 10, sizeof (svn_auth_provider_object_t *));

	/* The main disk-caching auth providers, for both
	   'username/password' creds and 'username' creds.  */
	svn_client_get_simple_provider (&provider, SVN_G(pool));
	APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;

	svn_client_get_username_provider (&provider, SVN_G(pool));
	APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;

	svn_client_get_ssl_server_trust_prompt_provider (&provider, php_svn_auth_ssl_client_server_trust_prompter, NULL, SVN_G(pool));
	APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;

	/* The server-cert, client-cert, and client-cert-password providers. */
	svn_client_get_ssl_server_trust_file_provider (&provider, SVN_G(pool));
	APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;

	svn_client_get_ssl_client_cert_file_provider (&provider, SVN_G(pool));
	APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;

	svn_client_get_ssl_client_cert_pw_file_provider (&provider, SVN_G(pool));
	APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;


	/* skip prompt stuff */
	svn_auth_open (&ab, providers, SVN_G(pool));
	/* turn off prompting */
	svn_auth_set_parameter(ab, SVN_AUTH_PARAM_NON_INTERACTIVE, "");
	/* turn off storing passwords */
	svn_auth_set_parameter(ab, SVN_AUTH_PARAM_DONT_STORE_PASSWORDS, "");
	SVN_G(ctx)->auth_baton = ab;

	return 0;
}

/* {{{ proto string svn_auth_get_parameter(string key)
   Retrieves authentication parameter at key */
PHP_FUNCTION(svn_auth_get_parameter)
{
	char *key;
	int keylen;
	const char *value;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &keylen)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();

	value = svn_auth_get_parameter(SVN_G(ctx)->auth_baton, key);
	if (value) {
		RETURN_STRING((char*)value, 1);
	}
}
/* }}} */

/* {{{ proto void svn_auth_set_parameter(string key, string value)
   Sets authentication parameter at key to value */

PHP_FUNCTION(svn_auth_set_parameter)
{
	char *key, *value;
	int keylen, valuelen;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &keylen, &value, &valuelen)) {
		return;
	}
	PHP_SVN_INIT_CLIENT();

	svn_auth_set_parameter(SVN_G(ctx)->auth_baton, apr_pstrdup(SVN_G(pool), key), apr_pstrdup(SVN_G(pool), value));
}
/* }}} */

/* {{{ proto bool svn_import(string path, string url, bool nonrecursive)
   Imports unversioned path into repository at url */

PHP_FUNCTION(svn_import)
{
	svn_client_commit_info_t *commit_info_p = NULL;
	char *path;
	int pathlen;
	char *url;
	int urllen;
	svn_boolean_t nonrecursive;
	svn_error_t *err;
	apr_pool_t *subpool;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssb",
				&path, &pathlen, &url, &urllen, &nonrecursive)) {
		RETURN_FALSE;
	}

	PHP_SVN_INIT_CLIENT();

	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_client_import(&commit_info_p, path, url, nonrecursive,
			SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error (err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);

}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(svn)
{
	zend_class_entry ce;
	zend_class_entry *ce_SvnWc;
	zend_class_entry *ce_SvnWcSchedule;
	zend_class_entry *ce_SvnNode;

	apr_initialize();
	ZEND_INIT_MODULE_GLOBALS(svn, php_svn_init_globals, NULL);

#define STRING_CONST(foo) REGISTER_STRING_CONSTANT(#foo, foo, CONST_CS|CONST_PERSISTENT)
#define LONG_CONST(foo) REGISTER_LONG_CONSTANT(#foo, foo, CONST_CS|CONST_PERSISTENT)



	INIT_CLASS_ENTRY(ce, "Svn", svn_methods);
	ce_Svn = zend_register_internal_class(&ce TSRMLS_CC);


	INIT_CLASS_ENTRY(ce, "SvnWc", NULL);
		ce_SvnWc = zend_register_internal_class(&ce TSRMLS_CC);

	INIT_CLASS_ENTRY(ce, "SvnWcSchedule", NULL);
		ce_SvnWcSchedule = zend_register_internal_class(&ce TSRMLS_CC);

	INIT_CLASS_ENTRY(ce, "SvnNode", NULL);
		ce_SvnNode = zend_register_internal_class(&ce TSRMLS_CC);




#define CLASS_CONST_LONG(class_name, const_name, value) \
		zend_declare_class_constant_long(ce_ ## class_name, const_name, \
			sizeof(const_name)-1, (long)value TSRMLS_CC);

	CLASS_CONST_LONG(Svn, "NON_RECURSIVE", SVN_NON_RECURSIVE);
	CLASS_CONST_LONG(Svn, "DISCOVER_CHANGED_PATHS", SVN_DISCOVER_CHANGED_PATHS);
	CLASS_CONST_LONG(Svn, "OMIT_MESSAGES", SVN_OMIT_MESSAGES);
	CLASS_CONST_LONG(Svn, "STOP_ON_COPY", SVN_STOP_ON_COPY);
	CLASS_CONST_LONG(Svn, "ALL", SVN_ALL);
	CLASS_CONST_LONG(Svn, "SHOW_UPDATES", SVN_SHOW_UPDATES);
	CLASS_CONST_LONG(Svn, "NO_IGNORE", SVN_NO_IGNORE);
	CLASS_CONST_LONG(Svn, "IGNORE_EXTERNALS", SVN_IGNORE_EXTERNALS);

	CLASS_CONST_LONG(Svn, "INITIAL", SVN_REVISION_INITIAL);
	CLASS_CONST_LONG(Svn, "HEAD", SVN_REVISION_HEAD);
	CLASS_CONST_LONG(Svn, "BASE", SVN_REVISION_BASE);
	CLASS_CONST_LONG(Svn, "COMMITTED", SVN_REVISION_COMMITTED);
	CLASS_CONST_LONG(Svn, "PREV", SVN_REVISION_PREV);


	CLASS_CONST_LONG(SvnWc, "NONE", svn_wc_status_none);
	CLASS_CONST_LONG(SvnWc, "UNVERSIONED", svn_wc_status_unversioned);
	CLASS_CONST_LONG(SvnWc, "NORMAL", svn_wc_status_normal);
	CLASS_CONST_LONG(SvnWc, "ADDED", svn_wc_status_added);
	CLASS_CONST_LONG(SvnWc, "MISSING", svn_wc_status_missing);
	CLASS_CONST_LONG(SvnWc, "DELETED", svn_wc_status_deleted);
	CLASS_CONST_LONG(SvnWc, "REPLACED", svn_wc_status_replaced);
	CLASS_CONST_LONG(SvnWc, "MODIFIED", svn_wc_status_modified);
	CLASS_CONST_LONG(SvnWc, "MERGED", svn_wc_status_merged);
	CLASS_CONST_LONG(SvnWc, "CONFLICTED", svn_wc_status_conflicted);
	CLASS_CONST_LONG(SvnWc, "IGNORED", svn_wc_status_ignored);
	CLASS_CONST_LONG(SvnWc, "OBSTRUCTED", svn_wc_status_obstructed);
	CLASS_CONST_LONG(SvnWc, "EXTERNAL", svn_wc_status_external);
	CLASS_CONST_LONG(SvnWc, "INCOMPLETE", svn_wc_status_incomplete);

	CLASS_CONST_LONG(SvnWcSchedule, "NORMAL", svn_wc_schedule_normal);
	CLASS_CONST_LONG(SvnWcSchedule, "ADD", svn_wc_schedule_add);
	CLASS_CONST_LONG(SvnWcSchedule, "DELETE", svn_wc_schedule_delete);
	CLASS_CONST_LONG(SvnWcSchedule, "REPLACE", svn_wc_schedule_replace);

	CLASS_CONST_LONG(SvnNode, "NONE", svn_node_none);
	CLASS_CONST_LONG(SvnNode, "FILE", svn_node_file);
	CLASS_CONST_LONG(SvnNode, "DIR", svn_node_dir);
	CLASS_CONST_LONG(SvnNode, "UNKNOWN", svn_node_unknown);



	STRING_CONST(SVN_AUTH_PARAM_DEFAULT_USERNAME);
	STRING_CONST(SVN_AUTH_PARAM_DEFAULT_PASSWORD);
	STRING_CONST(SVN_AUTH_PARAM_NON_INTERACTIVE);
	STRING_CONST(SVN_AUTH_PARAM_DONT_STORE_PASSWORDS);
	STRING_CONST(SVN_AUTH_PARAM_NO_AUTH_CACHE);
	STRING_CONST(SVN_AUTH_PARAM_SSL_SERVER_FAILURES);
	STRING_CONST(SVN_AUTH_PARAM_SSL_SERVER_CERT_INFO);
	STRING_CONST(SVN_AUTH_PARAM_CONFIG);
	STRING_CONST(SVN_AUTH_PARAM_SERVER_GROUP);
	STRING_CONST(SVN_AUTH_PARAM_CONFIG_DIR);
	STRING_CONST(PHP_SVN_AUTH_PARAM_IGNORE_SSL_VERIFY_ERRORS);
	STRING_CONST(SVN_FS_CONFIG_FS_TYPE);
	STRING_CONST(SVN_FS_TYPE_BDB);
	STRING_CONST(SVN_FS_TYPE_FSFS);
	STRING_CONST(SVN_PROP_REVISION_DATE);
	STRING_CONST(SVN_PROP_REVISION_ORIG_DATE);
	STRING_CONST(SVN_PROP_REVISION_AUTHOR);
	STRING_CONST(SVN_PROP_REVISION_LOG);

	LONG_CONST(SVN_REVISION_INITIAL);
	LONG_CONST(SVN_REVISION_HEAD);
	LONG_CONST(SVN_REVISION_BASE);
	LONG_CONST(SVN_REVISION_COMMITTED);
	LONG_CONST(SVN_REVISION_PREV);


	LONG_CONST(SVN_NON_RECURSIVE);   /* --non-recursive */
	LONG_CONST(SVN_DISCOVER_CHANGED_PATHS);    /* --verbose */
	LONG_CONST(SVN_OMIT_MESSAGES);    /* --quiet */
	LONG_CONST(SVN_STOP_ON_COPY);    /* --stop-on-copy */
	LONG_CONST(SVN_ALL);    /* --verbose in svn status */
	LONG_CONST(SVN_SHOW_UPDATES);   /* --show-updates */
	LONG_CONST(SVN_NO_IGNORE);   /* --no-ignore */

	LONG_CONST(svn_wc_status_none);
	LONG_CONST(svn_wc_status_unversioned);
	LONG_CONST(svn_wc_status_normal);
	LONG_CONST(svn_wc_status_added);
	LONG_CONST(svn_wc_status_missing);
	LONG_CONST(svn_wc_status_deleted);
	LONG_CONST(svn_wc_status_replaced);
	LONG_CONST(svn_wc_status_modified);
	LONG_CONST(svn_wc_status_merged);
	LONG_CONST(svn_wc_status_conflicted);
	LONG_CONST(svn_wc_status_ignored);
	LONG_CONST(svn_wc_status_obstructed);
	LONG_CONST(svn_wc_status_external);
	LONG_CONST(svn_wc_status_incomplete);

	LONG_CONST(svn_node_none);
	LONG_CONST(svn_node_file);
	LONG_CONST(svn_node_dir);
	LONG_CONST(svn_node_unknown);


	LONG_CONST(svn_wc_schedule_normal);
	LONG_CONST(svn_wc_schedule_add);
	LONG_CONST(svn_wc_schedule_delete);
	LONG_CONST(svn_wc_schedule_replace);



	le_svn_repos = zend_register_list_destructors_ex(php_svn_repos_dtor,
			NULL, "svn-repos", module_number);

	le_svn_fs = zend_register_list_destructors_ex(php_svn_fs_dtor,
			NULL, "svn-fs", module_number);

	le_svn_fs_root = zend_register_list_destructors_ex(php_svn_fs_root_dtor,
			NULL, "svn-fs-root", module_number);

	le_svn_repos_fs_txn = zend_register_list_destructors_ex(php_svn_repos_fs_txn_dtor,
			NULL, "svn-repos-fs-txn", module_number);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(svn)
{
	if (SVN_G(pool)) {
		svn_pool_destroy(SVN_G(pool));
		SVN_G(pool) = NULL;
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(svn)
{
	char vstr[128];

	php_info_print_table_start();
	php_info_print_table_header(2, "svn support", "enabled");

	php_svn_get_version(vstr, sizeof(vstr));

	php_info_print_table_row(2, "svn client version", vstr);
	php_info_print_table_row(2, "svn extension version", PHP_SVN_VERSION);
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* reference http://www.linuxdevcenter.com/pub/a/linux/2003/04/24/libsvn1.html */
/* {{{ proto bool Svn:checkout(string repository_url, string target_path [, int revision = Svn::HEAD, [, int flags])
   Check out a working copy from a repository */
/* }}} */
/* {{{ proto bool svn_checkout(string repository_url, string target_path [, int revision = Svn::HEAD, [, int flags])
   Checks out a particular revision from repos into targetpath */
PHP_FUNCTION(svn_checkout)
{
	char *repos_url = NULL, *target_path = NULL;
	const char *utf8_repos_url = NULL, *utf8_target_path = NULL;
	const char *can_repos_url = NULL, *can_target_path = NULL;
	int repos_url_len, target_path_len;
	svn_error_t *err;
	svn_opt_revision_t revision = { 0 }, peg_revision = { 0 };
	long flags = 0;
	apr_pool_t *subpool;

	revision.value.number = svn_opt_revision_unspecified;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|ll",
			&repos_url, &repos_url_len, &target_path, &target_path_len, &revision.value.number, &flags) == FAILURE) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}


	svn_utf_cstring_to_utf8 (&utf8_repos_url, repos_url, subpool);
	svn_utf_cstring_to_utf8 (&utf8_target_path, target_path, subpool);

	can_repos_url= svn_path_canonicalize(utf8_repos_url, subpool);
	can_target_path = svn_path_canonicalize(utf8_target_path, subpool);


	revision.kind = php_svn_get_revision_kind(revision);
	peg_revision.kind = svn_opt_revision_unspecified;

	err = svn_client_checkout2 (NULL,
			can_repos_url,
			can_target_path,
			&peg_revision,
			&revision,
			!(flags & SVN_NON_RECURSIVE),
			flags & SVN_IGNORE_EXTERNALS,
			SVN_G(ctx),
			subpool);

	if (err) {
		php_svn_handle_error (err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */
/* {{{ proto string Svn::cat(string url[, int revision = Svn::HEAD])
   Returns the contents of the specified URL (for listing the contents of directories, use Svn::list()) */
/* }}} */
/* {{{ proto string svn_cat(string repos_url[, int revision_no])
   Returns the contents of repos_url, optionally at revision_no */
PHP_FUNCTION(svn_cat)
{
	char *url = NULL;
	int url_len;
	apr_size_t size;
	svn_error_t *err;
	svn_opt_revision_t revision = { 0 }, peg_revision = { 0 } ;
	svn_stream_t *out = NULL;
	svn_stringbuf_t *buf = NULL;
	char *retdata =NULL;
	apr_pool_t *subpool;
	const char *true_path;

	revision.value.number = svn_opt_revision_unspecified;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l",
		&url, &url_len, &revision.value.number) == FAILURE) {
		return;
	}
	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	RETVAL_FALSE;

	revision.kind = php_svn_get_revision_kind(revision);

	buf = svn_stringbuf_create("", subpool);
	if (!buf) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to allocate stringbuf");
		goto cleanup;
	}

	out = svn_stream_from_stringbuf(buf, subpool);
	if (!out) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to create svn stream");
		goto cleanup;
	}
	/* do we need to utf8 / canonize the path ? */
	err = svn_opt_parse_path(&peg_revision, &true_path, url, subpool);
	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		goto cleanup;
	}

	err = svn_client_cat2(out, true_path, &peg_revision, &revision, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		goto cleanup;
	}

	retdata = emalloc(buf->len + 1);
	size = buf->len;
	err = svn_stream_read(out, retdata, &size);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		goto cleanup;
	}

	retdata[size] = '\0';
	RETURN_STRINGL(retdata, size, 0);
	retdata = NULL;

cleanup:
	svn_pool_destroy(subpool);
	if (retdata) efree(retdata);
}

/* }}} */


/* {{{ proto array svn_ls(string repos_url[, int revision_no])
   Returns list of directory contents in repos_url, optionally at revision_no */
PHP_FUNCTION(svn_ls)
{
	char *repos_url = NULL;
	int repos_url_len,  revision_no = -1;
	svn_error_t *err;
	svn_opt_revision_t revision = { 0 };
	apr_hash_t *dirents;
	apr_array_header_t *array;
	int i;
	apr_pool_t *subpool;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l",
			&repos_url, &repos_url_len, &revision_no) == FAILURE) {
		return;
	}
	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}
	RETVAL_FALSE;

	if (revision_no <= 0) {
		revision.kind = svn_opt_revision_head;
	} else {
		revision.kind = svn_opt_revision_number;
		revision.value.number = revision_no ;
	}

	/* grab the most recent version of the website. */
	err = svn_client_ls (&dirents,
                repos_url,
                &revision,
                FALSE,
                SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		goto cleanup;
	}

	array = svn_sort__hash (dirents, svn_sort_compare_items_as_paths, subpool);
	array_init(return_value);

	for (i = 0; i < array->nelts; ++i)
	{
		const char *utf8_entryname;
		svn_dirent_t *dirent;
		svn_sort__item_t *item;
		apr_time_t now = apr_time_now();
		apr_time_exp_t exp_time;
		apr_status_t apr_err;
		apr_size_t size;
		char timestr[20];
		const char   *utf8_timestr;
		zval 	*row;

		item = &APR_ARRAY_IDX (array, i, svn_sort__item_t);
		utf8_entryname = item->key;
		dirent = apr_hash_get (dirents, utf8_entryname, item->klen);

		/* svn_time_to_human_cstring gives us something *way* too long
		to use for this, so we have to roll our own.  We include
		the year if the entry's time is not within half a year. */
		apr_time_exp_lt (&exp_time, dirent->time);
		if (apr_time_sec(now - dirent->time) < (365 * 86400 / 2)
			&& apr_time_sec(dirent->time - now) < (365 * 86400 / 2))
		{
			apr_err = apr_strftime (timestr, &size, sizeof (timestr),
				      "%b %d %H:%M", &exp_time);
		} else {
			apr_err = apr_strftime (timestr, &size, sizeof (timestr),
				      "%b %d %Y", &exp_time);
		}

		/* if that failed, just zero out the string and print nothing */
		if (apr_err)
			timestr[0] = '\0';

		/* we need it in UTF-8. */
		svn_utf_cstring_to_utf8 (&utf8_timestr, timestr, subpool);

		MAKE_STD_ZVAL(row);
		array_init(row);
		add_assoc_long(row,   "created_rev", 	(long) dirent->created_rev);
		add_assoc_string(row, "last_author", 	dirent->last_author ? (char *) dirent->last_author : " ? ", 1);
		add_assoc_long(row,   "size", 		dirent->size);
		add_assoc_string(row, "time", 		timestr,1);
		add_assoc_long(row,   "time_t", 	apr_time_sec(dirent->time));
		/* this doesnt have a matching struct name */
		add_assoc_string(row, "name", 		(char *) utf8_entryname,1);
		/* should this be a integer or something? - not very clear though.*/
		add_assoc_string(row, "type", 		(dirent->kind == svn_node_dir) ? "dir" : "file",1);
		add_next_index_zval(return_value,row);
	}

cleanup:
	svn_pool_destroy(subpool);

}
/* }}} */
static svn_error_t *
php_svn_log_receiver (	void *ibaton,
				apr_hash_t *changed_paths,
				svn_revnum_t rev,
				const char *author,
				const char *date,
				const char *msg,
				apr_pool_t *pool)
{
	struct php_svn_log_receiver_baton *baton = (struct php_svn_log_receiver_baton*) ibaton;
	zval  *row, *paths;
	apr_array_header_t *sorted_paths;
	int i;
	TSRMLS_FETCH();

	if (rev == 0) {
		return SVN_NO_ERROR;
	}

	MAKE_STD_ZVAL(row);
	array_init(row);
	add_assoc_long(row, "rev", (long) rev);

	if (author) {
		add_assoc_string(row, "author", (char *) author, 1);
	}
	if (!baton->omit_messages && msg) {
		add_assoc_string(row, "msg", (char *) msg, 1);
	}
	if (date) {
		add_assoc_string(row, "date", (char *) date, 1);
	}

	if (changed_paths) {


		MAKE_STD_ZVAL(paths);
		array_init(paths);

		sorted_paths = svn_sort__hash(changed_paths, svn_sort_compare_items_as_paths, pool);

		for (i = 0; i < sorted_paths->nelts; i++)
		{
			svn_sort__item_t *item;
			svn_log_changed_path_t *log_item;
			zval *zpaths;
			const char *path;

			MAKE_STD_ZVAL(zpaths);
			array_init(zpaths);
			item = &(APR_ARRAY_IDX (sorted_paths, i, svn_sort__item_t));
			path = item->key;
			log_item = apr_hash_get (changed_paths, item->key, item->klen);

			add_assoc_stringl(zpaths, "action", &(log_item->action), 1,1);
			add_assoc_string(zpaths, "path", (char *) item->key, 1);

			if (log_item->copyfrom_path
					&& SVN_IS_VALID_REVNUM (log_item->copyfrom_rev)) {
				add_assoc_string(zpaths, "copyfrom", (char *) log_item->copyfrom_path, 1);
				add_assoc_long(zpaths, "rev", (long) log_item->copyfrom_rev);
			} else {

			}

			add_next_index_zval(paths,zpaths);
		}
		add_assoc_zval(row,"paths",paths);
	}

	add_next_index_zval(baton->result, row);
	return SVN_NO_ERROR;
}
/* {{{ proto array Svn::log(string url[, int start_revision =  Svn::HEAD, [, int end_revision = Svn::INITIAL [, int limit [, int flags ]]]])
   Returns commit log messages */
/* }}} */
/* {{{ proto array svn_log(string repos_url[, int start_revision =  Svn::HEAD, [, int end_revision = Svn::INITIAL [, int limit [, int flags ]]]])
   Returns the commit log messages of repos_url */
PHP_FUNCTION(svn_log)
{
	const char *url = NULL, *utf8_url = NULL;
	int url_len;

	svn_error_t *err;
	svn_opt_revision_t 	start_revision = { 0 }, end_revision = { 0 };

	apr_array_header_t *targets;

	apr_pool_t *subpool;
	long limit = 0;
	long flags = SVN_DISCOVER_CHANGED_PATHS | SVN_STOP_ON_COPY;
	struct php_svn_log_receiver_baton baton;

	start_revision.value.number = svn_opt_revision_unspecified;
	end_revision.value.number = svn_opt_revision_unspecified;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|llll",
			&url, &url_len,
			&start_revision.value.number, &end_revision.value.number,
			&limit,  &flags) == FAILURE) {
		return;
	}
	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}
	RETVAL_FALSE;

	svn_utf_cstring_to_utf8 (&utf8_url, url, subpool);


	if ((ZEND_NUM_ARGS() > 2) && (end_revision.value.number == svn_opt_revision_unspecified)) {
		end_revision.value.number = SVN_REVISION_INITIAL;
	}

	start_revision.kind = php_svn_get_revision_kind(start_revision);

	if (start_revision.value.number == svn_opt_revision_unspecified) {
		end_revision.kind = svn_opt_revision_number;
	} else if (end_revision.value.number == svn_opt_revision_unspecified) {
		end_revision = start_revision;
	} else {
 		end_revision.kind = php_svn_get_revision_kind(end_revision);
 	}



	targets = apr_array_make (subpool, 1, sizeof(char *));

	APR_ARRAY_PUSH(targets, const char *) =
		svn_path_canonicalize(utf8_url, subpool);
	array_init(return_value);
	baton.result = (zval *)return_value;
	baton.omit_messages = flags & SVN_OMIT_MESSAGES;

	err = svn_client_log2(
		targets,
		&start_revision,
		&end_revision,
		limit,
		flags & SVN_DISCOVER_CHANGED_PATHS,
		flags & SVN_STOP_ON_COPY,
		php_svn_log_receiver,
		(void *) &baton,
		SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

static size_t php_apr_file_write(php_stream *stream, const char *buf, size_t count TSRMLS_DC)
{
	apr_file_t *thefile = (apr_file_t*)stream->abstract;
	apr_size_t nbytes = (apr_size_t)count;

	apr_file_write(thefile, buf, &nbytes);

	return (size_t)nbytes;
}

static size_t php_apr_file_read(php_stream *stream, char *buf, size_t count TSRMLS_DC)
{
	apr_file_t *thefile = (apr_file_t*)stream->abstract;
	apr_size_t nbytes = (apr_size_t)count;

	apr_file_read(thefile, buf, &nbytes);

	if (nbytes == 0) stream->eof = 1;

	return (size_t)nbytes;
}

static int php_apr_file_close(php_stream *stream, int close_handle TSRMLS_DC)
{
	if (close_handle) {
		apr_file_close((apr_file_t*)stream->abstract);
	}
	return 0;
}

static int php_apr_file_flush(php_stream *stream TSRMLS_DC)
{
	apr_file_flush((apr_file_t*)stream->abstract);
	return 0;
}

static int php_apr_file_seek(php_stream *stream, off_t offset, int whence, off_t *newoffset TSRMLS_DC)
{
	apr_file_t *thefile = (apr_file_t*)stream->abstract;
	apr_off_t off = (apr_off_t)offset;

	/* NB: apr_seek_where_t is defined using the standard SEEK_XXX whence values */
	apr_file_seek(thefile, whence, &off);

	*newoffset = (off_t)off;
	return 0;
}

static php_stream_ops php_apr_stream_ops = {
	php_apr_file_write,
	php_apr_file_read,
	php_apr_file_close,
	php_apr_file_flush,
	"svn diff stream",
	php_apr_file_seek,
	NULL, /* cast */
	NULL, /* stat */
	NULL /* set_option */
};

/* {{{ proto array svn_diff(string path1, int rev1, string path2, int rev2)
   Recursively diffs two paths.  Returns an array consisting of two streams: the first is the diff output and the second contains error stream output */
PHP_FUNCTION(svn_diff)
{
	const char *tmp_dir;
	char outname[256], errname[256];
	apr_pool_t *subpool;
	apr_file_t *outfile = NULL, *errfile = NULL;
	svn_error_t *err;
	char *path1, *path2;
	const char *utf8_path1 = NULL,*utf8_path2 = NULL;
	const char *can_path1 = NULL,*can_path2 = NULL;
	int path1len, path2len;
	long rev1 = -1, rev2 = -1;
	apr_array_header_t diff_options = { 0, 0, 0, 0, 0};
	svn_opt_revision_t revision1, revision2;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl!sl!",
			&path1, &path1len, &rev1,
			&path2, &path2len, &rev2)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}
	RETVAL_FALSE;

	if (rev1 <= 0) {
		revision1.kind = svn_opt_revision_head;
	} else {
		revision1.kind = svn_opt_revision_number;
		revision1.value.number = rev1;
	}
	if (rev2 <= 0) {
		revision2.kind = svn_opt_revision_head;
	} else {
		revision2.kind = svn_opt_revision_number;
		revision2.value.number = rev2;
	}

 	apr_temp_dir_get(&tmp_dir, subpool);
	sprintf(outname, "%s/phpsvnXXXXXX", tmp_dir);
	sprintf(errname, "%s/phpsvnXXXXXX", tmp_dir);

	/* use global pool, so stream lives after this function call */
	apr_file_mktemp(&outfile, outname,
			APR_CREATE|APR_READ|APR_WRITE|APR_EXCL|APR_DELONCLOSE,
			SVN_G(pool));

	/* use global pool, so stream lives after this function call */
	apr_file_mktemp(&errfile, errname,
			APR_CREATE|APR_READ|APR_WRITE|APR_EXCL|APR_DELONCLOSE,
			SVN_G(pool));

	svn_utf_cstring_to_utf8 (&utf8_path1, path1, subpool);
	svn_utf_cstring_to_utf8 (&utf8_path2, path2, subpool);

	can_path1= svn_path_canonicalize(utf8_path1, subpool);
	can_path2= svn_path_canonicalize(utf8_path2, subpool);

	err = svn_client_diff(&diff_options,
			can_path1, &revision1,
			can_path2, &revision2,
			1,
			0,
			0,
			outfile, errfile,
			SVN_G(ctx), subpool);

	if (err) {
		apr_file_close(errfile);
		apr_file_close(outfile);
		php_svn_handle_error(err TSRMLS_CC);
	} else {
		zval *t;
		php_stream *stm = NULL;
		apr_off_t off = (apr_off_t)0;

		array_init(return_value);

		/* set the file pointer to the beginning of the file */
		apr_file_seek(outfile, APR_SET, &off);
		apr_file_seek(errfile, APR_SET, &off);

		/* 'bless' the apr files into streams and return those */
		stm = php_stream_alloc(&php_apr_stream_ops, outfile, 0, "rw");
		MAKE_STD_ZVAL(t);
		php_stream_to_zval(stm, t);
		add_next_index_zval(return_value, t);

		stm = php_stream_alloc(&php_apr_stream_ops, errfile, 0, "rw");
		MAKE_STD_ZVAL(t);
		php_stream_to_zval(stm, t);
		add_next_index_zval(return_value, t);
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto bool svn_cleanup(string workingdir)
   Recursively cleanup a working copy directory, finishing any incomplete operations, removing lockfiles, etc. */
PHP_FUNCTION(svn_cleanup)
{
	char *workingdir;
	int len;
	svn_error_t *err;
	apr_pool_t *subpool;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &workingdir, &len)) {
		RETURN_FALSE;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_client_cleanup(workingdir, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */
/* {{{ proto bool svn_revert(string path[, bool recursive = false]) */
PHP_FUNCTION(svn_revert)
{
	const char *path = NULL, *utf8_path = NULL;
	long pathlen;
	zend_bool recursive = 0;
	svn_error_t *err;
	apr_array_header_t *targets;
	apr_pool_t *subpool;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &path, &pathlen, &recursive) ) {
		RETURN_FALSE;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}
	RETVAL_FALSE;

	svn_utf_cstring_to_utf8 (&utf8_path, path, subpool);

	targets = apr_array_make (subpool, 1, sizeof(char *));

	APR_ARRAY_PUSH(targets, const char *) = svn_path_canonicalize(utf8_path, subpool);

	err = svn_client_revert(
			targets,
			recursive,
			SVN_G(ctx),
			subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto bool svn_resolved(string path[, bool recursive = false]) */
PHP_FUNCTION(svn_resolved)
{
	const char *path = NULL, *utf8_path = NULL;
	long pathlen;
	zend_bool recursive = 0;
	svn_error_t *err;
	apr_pool_t *subpool;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &path, &pathlen, &recursive) ) {
		RETURN_FALSE;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}
	RETVAL_FALSE;

	svn_utf_cstring_to_utf8 (&utf8_path, path, subpool);

	path = svn_path_canonicalize(utf8_path, subpool);

	err = svn_client_resolved(
			path,
			recursive,
			SVN_G(ctx),
			subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */




static int replicate_hash(void *pDest, int num_args, va_list args, zend_hash_key *key)
{
	zval **val = (zval **)pDest;
	apr_hash_t *hash = va_arg(args, apr_hash_t*);

	if (key->nKeyLength && Z_TYPE_PP(val) == IS_STRING) {
		/* apr doesn't want the NUL terminator in its keys */
		apr_hash_set(hash, key->arKey, key->nKeyLength-1, Z_STRVAL_PP(val));
	}

	va_end(args);

	return ZEND_HASH_APPLY_KEEP;
}

static apr_hash_t *replicate_zend_hash_to_apr_hash(zval *arr, apr_pool_t *pool TSRMLS_DC)
{
	apr_hash_t *hash;

	if (!arr) return NULL;

	hash = apr_hash_make(pool);

	zend_hash_apply_with_arguments(Z_ARRVAL_P(arr), replicate_hash, 1, hash);

	return hash;
}

/* {{{ proto string svn_fs_revision_prop(resource fs, int revnum, string propname)
   Fetches the value of a named property */
PHP_FUNCTION(svn_fs_revision_prop)
{
	zval *zfs;
	long revnum;
	struct php_svn_fs *fs;
	svn_error_t *err;
	svn_string_t *str;
	char *propname;
	int propnamelen;
	apr_pool_t *subpool;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rls",
				&zfs, &revnum, &propname, &propnamelen)) {
		return;
	}

	ZEND_FETCH_RESOURCE(fs, struct php_svn_fs *, &zfs, -1, "svn-fs", le_svn_fs);

	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_fs_revision_prop(&str, fs->fs, revnum, propname, subpool);
	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	}

	RETVAL_STRINGL((char*)str->data, str->len, 1);

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto int svn_fs_youngest_rev(resource fs)
   Returns the number of the youngest revision in the filesystem */
PHP_FUNCTION(svn_fs_youngest_rev)
{
	zval *zfs;
	struct php_svn_fs *fs;
	svn_error_t *err;
	svn_revnum_t revnum;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
				&zfs)) {
		return;
	}

	ZEND_FETCH_RESOURCE(fs, struct php_svn_fs *, &zfs, -1, "svn-fs", le_svn_fs);

	err = svn_fs_youngest_rev(&revnum, fs->fs, fs->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	}

	RETURN_LONG(revnum);
}
/* }}} */


/* {{{ proto resource svn_fs_revision_root(resource fs, int revnum)
   Get a handle on a specific version of the repository root */
PHP_FUNCTION(svn_fs_revision_root)
{
	zval *zfs;
	long revnum;
	struct php_svn_fs *fs;
	svn_fs_root_t *root;
	svn_error_t *err;
	struct php_svn_fs_root *resource;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl",
				&zfs, &revnum)) {
		return;
	}

	ZEND_FETCH_RESOURCE(fs, struct php_svn_fs *, &zfs, -1, "svn-fs", le_svn_fs);

	err = svn_fs_revision_root(&root, fs->fs, revnum, fs->repos->pool);
	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	}

	resource = emalloc(sizeof(*resource));
	resource->root = root;
	resource->repos = fs->repos;
	zend_list_addref(fs->repos->rsrc_id);
	ZEND_REGISTER_RESOURCE(return_value, resource, le_svn_fs_root);
}
/* }}} */

/* {{{ proto resource svn_fs_file_contents(resource fsroot, string path)
   Returns a stream to access the contents of a file from a given version of the fs */

static size_t php_svn_stream_write(php_stream *stream, const char *buf, size_t count TSRMLS_DC)
{
	svn_stream_t *thefile = (svn_stream_t*)stream->abstract;
	apr_size_t nbytes = (apr_size_t)count;

	svn_stream_write(thefile, buf, &nbytes);

	return (size_t)nbytes;
}

static size_t php_svn_stream_read(php_stream *stream, char *buf, size_t count TSRMLS_DC)
{
	svn_stream_t *thefile = (svn_stream_t*)stream->abstract;
	apr_size_t nbytes = (apr_size_t)count;

	svn_stream_read(thefile, buf, &nbytes);

	if (nbytes == 0) stream->eof = 1;

	return (size_t)nbytes;
}

static int php_svn_stream_close(php_stream *stream, int close_handle TSRMLS_DC)
{
	if (close_handle) {
		svn_stream_close((svn_stream_t*)stream->abstract);
	}
	return 0;
}

static int php_svn_stream_flush(php_stream *stream TSRMLS_DC)
{
	return 0;
}

static php_stream_ops php_svn_stream_ops = {
	php_svn_stream_write,
	php_svn_stream_read,
	php_svn_stream_close,
	php_svn_stream_flush,
	"svn content stream",
	NULL, /* seek */
	NULL, /* cast */
	NULL, /* stat */
	NULL /* set_option */
};


PHP_FUNCTION(svn_fs_file_contents)
{
	zval *zfsroot;
	struct php_svn_fs_root *fsroot;
	char *path;
	int pathlen;
	svn_error_t *err;
	svn_stream_t *svnstm;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zfsroot, &path, &pathlen)) {
		return;
	}

	ZEND_FETCH_RESOURCE(fsroot, struct php_svn_fs_root*, &zfsroot, -1, "svn-fs-root", le_svn_fs_root);

	err = svn_fs_file_contents(&svnstm, fsroot->root, path, SVN_G(pool));

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	} else {
		php_stream *stm;
		stm = php_stream_alloc(&php_svn_stream_ops, svnstm, 0, "r");
		php_stream_to_zval(stm, return_value);
	}
}
/* }}} */

/* {{{ proto int svn_fs_file_length(resource fsroot, string path)
   Returns the length of a file from a given version of the fs */
PHP_FUNCTION(svn_fs_file_length)
{
	zval *zfsroot;
	struct php_svn_fs_root *fsroot;
	char *path;
	int pathlen;
	svn_filesize_t len;
	svn_error_t *err;
	apr_pool_t *subpool;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zfsroot, &path, &pathlen)) {
		return;
	}

	ZEND_FETCH_RESOURCE(fsroot, struct php_svn_fs_root*, &zfsroot, -1, "svn-fs-root", le_svn_fs_root);

	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_fs_file_length(&len, fsroot->root, path, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		/* TODO: 64 bit */
		RETVAL_LONG(len);
	}
	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto int svn_fs_node_prop(resource fsroot, string path, string propname)
   Returns the value of a property for a node */
PHP_FUNCTION(svn_fs_node_prop)
{
	zval *zfsroot;
	struct php_svn_fs_root *fsroot;
	char *path, *propname;
	int pathlen, propnamelen;
	svn_error_t *err;
	apr_pool_t *subpool;
	svn_string_t *val;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rss",
				&zfsroot, &path, &pathlen, &propname, &propnamelen)) {
		return;
	}

	ZEND_FETCH_RESOURCE(fsroot, struct php_svn_fs_root*, &zfsroot, -1, "svn-fs-root", le_svn_fs_root);

	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_fs_node_prop(&val, fsroot->root, path, propname, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		if (val != NULL && val->data != NULL) {
			RETVAL_STRINGL((char *)val->data, val->len, 1);
		} else {
			RETVAL_EMPTY_STRING();
		}
	}
	svn_pool_destroy(subpool);
}
/* }}} */


/* {{{ proto int svn_fs_node_created_rev(resource fsroot, string path)
   Returns the revision in which path under fsroot was created */
PHP_FUNCTION(svn_fs_node_created_rev)
{
	zval *zfsroot;
	struct php_svn_fs_root *fsroot;
	char *path;
	int pathlen;
	svn_error_t *err;
	apr_pool_t *subpool;
	svn_revnum_t rev;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zfsroot, &path, &pathlen)) {
		return;
	}

	ZEND_FETCH_RESOURCE(fsroot, struct php_svn_fs_root*, &zfsroot, -1, "svn-fs-root", le_svn_fs_root);

	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_fs_node_created_rev(&rev, fsroot->root, path, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_LONG(rev);
	}
	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto array svn_fs_dir_entries(resource fsroot, string path)
   Enumerates the directory entries under path; returns a hash of dir names to file type */
PHP_FUNCTION(svn_fs_dir_entries)
{
	zval *zfsroot;
	struct php_svn_fs_root *fsroot;
	char *path;
	int pathlen;
	svn_error_t *err;
	apr_pool_t *subpool;
	apr_hash_t *hash;
	apr_hash_index_t *hi;
	union {
		void *vptr;
		svn_fs_dirent_t *ent;
	} pun;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zfsroot, &path, &pathlen)) {
		return;
	}

	ZEND_FETCH_RESOURCE(fsroot, struct php_svn_fs_root*, &zfsroot, -1, "svn-fs-root", le_svn_fs_root);

	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_fs_dir_entries(&hash, fsroot->root, path, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		array_init(return_value);

		for (hi = apr_hash_first(subpool, hash); hi; hi = apr_hash_next(hi)) {
			apr_hash_this(hi, NULL, NULL, &pun.vptr);
			add_assoc_long(return_value, (char*)pun.ent->name, pun.ent->kind);
		}
	}
	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto int svn_fs_check_path(resource fsroot, string path)
   Determines what kind of item lives at path in a given repository fsroot */
PHP_FUNCTION(svn_fs_check_path)
{
	zval *zfsroot;
	svn_node_kind_t kind;
	struct php_svn_fs_root *fsroot;
	char *path;
	int pathlen;
	svn_error_t *err;
	apr_pool_t *subpool;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zfsroot, &path, &pathlen)) {
		return;
	}

	ZEND_FETCH_RESOURCE(fsroot, struct php_svn_fs_root*, &zfsroot, -1, "svn-fs-root", le_svn_fs_root);

	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_fs_check_path(&kind, fsroot->root, path, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_LONG(kind);
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto resource svn_repos_fs(resource repos)
   Gets a handle on the filesystem for a repository */
PHP_FUNCTION(svn_repos_fs)
{
	struct php_svn_repos *repos = NULL;
	struct php_svn_fs *resource = NULL;
	zval *zrepos;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
				&zrepos)) {
		return;
	}

	ZEND_FETCH_RESOURCE(repos, struct php_svn_repos *, &zrepos, -1, "svn-repos", le_svn_repos);

	resource = emalloc(sizeof(*resource));
	resource->repos = repos;
	zend_list_addref(repos->rsrc_id);
	resource->fs = svn_repos_fs(repos->repos);

	ZEND_REGISTER_RESOURCE(return_value, resource, le_svn_fs);
}
/* }}} */

/* {{{ proto resource svn_repos_open(string path)
   Open a shared lock on a repository. */
PHP_FUNCTION(svn_repos_open)
{
	char *path;
	int pathlen;
	apr_pool_t *subpool;
	svn_error_t *err;
	svn_repos_t *repos = NULL;
	struct php_svn_repos *resource = NULL;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
				&path, &pathlen)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_repos_open(&repos, path, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
	}

	if (repos) {
		resource = emalloc(sizeof(*resource));
		resource->pool = subpool;
		resource->repos = repos;
		ZEND_REGISTER_RESOURCE(return_value, resource, le_svn_repos);
	} else {
		svn_pool_destroy(subpool);
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array svn_info(string path, bool recurse = false)
   Returns subversion information about a path */
static svn_error_t *info_func (void *baton, const char *path, const svn_info_t *info, apr_pool_t *pool) {
	zval *return_value = (zval*)baton;
	zval *entry;
	TSRMLS_FETCH();

	MAKE_STD_ZVAL(entry);
	array_init(entry);

	add_assoc_string(entry, "path", (char*)path, 1);
	if (info) {
		if (info->URL) {
			add_assoc_string(entry, "url", (char *)info->URL, 1);
		}

		add_assoc_long(entry, "revision", info->rev);
		add_assoc_long(entry, "kind", info->kind);

		if (info->repos_root_URL) {
			add_assoc_string(entry, "repos", (char *)info->repos_root_URL, 1);
		}

		add_assoc_long(entry, "last_changed_rev", info->last_changed_rev);
		add_assoc_string(entry, "last_changed_date", (char *) svn_time_to_cstring(info->last_changed_date, pool), 1);

		if (info->last_changed_author) {
			add_assoc_string(entry, "last_changed_author", (char *)info->last_changed_author, 1);
		}

		if (info->lock) {
			add_assoc_bool(entry, "locked", 1);
		}

		if (info->has_wc_info) {
			add_assoc_bool(entry, "is_working_copy", 1);
		}
	}

	add_next_index_zval(return_value, entry);

	return NULL;
}

PHP_FUNCTION(svn_info)
{
	const char *path = NULL, *utf8_path = NULL;
	int pathlen;
	apr_pool_t *subpool;
	zend_bool recurse = 1;
	svn_error_t *err;
	svn_opt_revision_t rev;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b",
				&path, &pathlen, &recurse)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));

	if (!subpool) {
		RETURN_FALSE;
	}

	svn_utf_cstring_to_utf8 (&utf8_path, path, subpool);
	path = svn_path_canonicalize(utf8_path, subpool);

	array_init(return_value);
	rev.kind = svn_opt_revision_head;

	err = svn_client_info (path, NULL, NULL, info_func, return_value, recurse, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto resource svn_export(string frompath, string topath [,bool working_copy = true])
   Exports a clean directory tree from the repository specified into the path provided */
PHP_FUNCTION(svn_export)
{
	const char *from = NULL, *to = NULL;
	const char *utf8_from_path = NULL, *utf8_to_path = NULL;
	int fromlen, tolen;
	apr_pool_t *subpool;
	zend_bool working_copy = 1;
	svn_error_t *err;
	svn_opt_revision_t revision, peg_revision;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|b",
				&from, &fromlen, &to, &tolen, &working_copy)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));

	if (!subpool) {
		RETURN_FALSE;
	}

	svn_utf_cstring_to_utf8 (&utf8_from_path, from, subpool);
	svn_utf_cstring_to_utf8 (&utf8_to_path, to, subpool);

	from = svn_path_canonicalize(utf8_from_path, subpool);
	to = svn_path_canonicalize(utf8_to_path, subpool);

	if (working_copy) {
		revision.kind = svn_opt_revision_working;
	} else {
		revision.kind = svn_opt_revision_head;
	}

	peg_revision.kind = svn_opt_revision_unspecified;

	err = svn_client_export3(NULL, from, to, &peg_revision, &revision, TRUE, FALSE, TRUE, NULL, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto resource svn_switch(string path, string url [,bool working_copy = true])
   */
PHP_FUNCTION(svn_switch)
{
	char *url, *path;
	int urllen, pathlen;
	apr_pool_t *subpool;
	zend_bool working_copy = 1;
	svn_error_t *err;
	svn_opt_revision_t revision;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|b",
					&path, &pathlen, &url, &urllen, &working_copy)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));

	if (!subpool) {
		RETURN_FALSE;
	}

	if (working_copy) {
		revision.kind = svn_opt_revision_working;
	} else {
		revision.kind = svn_opt_revision_head;
	}

	err = svn_client_switch(NULL, (const char*)path, (const char*)url, &revision, TRUE, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto resource svn_copy(string log, string src_path, string to_path [,bool working_copy = true])
   */
PHP_FUNCTION(svn_copy)
{
	char *src_path, *dst_path, *log;
	int src_pathlen, dst_pathlen, loglen ;
	apr_pool_t *subpool;
	zend_bool working_copy = 1;
	svn_error_t *err;
	svn_commit_info_t *info = NULL;
	svn_opt_revision_t revision;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss|b",
					&log, &loglen, &src_path, &src_pathlen, &dst_path, &dst_pathlen, &working_copy)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));

	if (!subpool) {
		RETURN_FALSE;
	}

	if (working_copy) {
		revision.kind = svn_opt_revision_working;
	} else {
		revision.kind = svn_opt_revision_head;
	}

	SVN_G(ctx)->log_msg_baton = log;

	err = svn_client_copy3(&info, (const char*)src_path, &revision, (const char*)dst_path, SVN_G(ctx), subpool);
	SVN_G(ctx)->log_msg_baton = NULL;

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else if (info) {
		array_init(return_value);
		add_next_index_long(return_value, info->revision);
		add_next_index_string(return_value, (char*)info->date, 1);
		add_next_index_string(return_value, (char*)info->author, 1);
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "commit didn't return any info");
		RETVAL_FALSE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */


static svn_error_t *
php_svn_blame_message_receiver (void *baton,
				apr_int64_t line_no,
				svn_revnum_t rev,
				const char *author,
				const char *date,
				const char *line,
				apr_pool_t *pool)
{
	zval *return_value = (zval *)baton, *row;

	TSRMLS_FETCH();

	if (rev == 0) {
		return SVN_NO_ERROR;
	}

	MAKE_STD_ZVAL(row);
	array_init(row);


	add_assoc_long(row, "rev", (long) rev);
	add_assoc_long(row, "line_no", line_no + 1);
	add_assoc_string(row, "line", (char *) line, 1);

	if (author) {
		add_assoc_string(row, "author", (char *) author, 1);
	}
	if (date) {
		add_assoc_string(row, "date", (char *) date, 1);
	}


	add_next_index_zval(return_value, row);
	return SVN_NO_ERROR;
}

/* {{{ proto array svn_blame(string repos_url[, int revision_no])
 */
PHP_FUNCTION(svn_blame)
{
	const char *repos_url = NULL;
	int repos_url_len;
	int revision = -1;
	svn_error_t *err;
	svn_opt_revision_t
			start_revision = { 0 },
			end_revision = { 0 },
			peg_revision;
	svn_diff_file_options_t diff_options;
	svn_boolean_t ignore_mime_type = TRUE;
	apr_pool_t *subpool;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &repos_url, &repos_url_len, &revision) == FAILURE) {
		RETURN_FALSE;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	RETVAL_FALSE;

	if (revision == -1) {
		start_revision.kind =  svn_opt_revision_head;
		end_revision.kind   =  svn_opt_revision_head;
	} else {
		start_revision.kind =  svn_opt_revision_number;
		start_revision.value.number = revision ;

		end_revision.kind   =  svn_opt_revision_number;
		end_revision.value.number = revision ;
	}
	peg_revision.kind = svn_opt_revision_unspecified;
	diff_options.ignore_space = svn_diff_file_ignore_space_none;
	diff_options.ignore_eol_style = FALSE;

	array_init(return_value);

	err = svn_client_blame3(
			repos_url,
			&peg_revision,
			&start_revision,
			&end_revision,
			&diff_options,
			ignore_mime_type,
			php_svn_blame_message_receiver,
			(void *) return_value,
			SVN_G(ctx),
			subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto mixed svn_delete(string path [,bool force = true])
	Delete a file from a repository
   */
PHP_FUNCTION(svn_delete)
{
	const char *path = NULL, *utf8_path = NULL;
	int pathlen;
	apr_pool_t *subpool;
	zend_bool force = 0;
	svn_error_t *err;
	svn_commit_info_t *info = NULL;
	apr_array_header_t *targets;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b",
					&path, &pathlen, &force)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));

	if (!subpool) {
		RETURN_FALSE;
	}

	svn_utf_cstring_to_utf8 (&utf8_path, path, subpool);

	targets = apr_array_make (subpool, 1, sizeof(char *));

	APR_ARRAY_PUSH(targets, const char *) = svn_path_canonicalize(utf8_path, subpool);
	err = svn_client_delete2(&info, targets, force, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else if (info) {
		array_init(return_value);
		add_next_index_long(return_value, info->revision);
		add_next_index_string(return_value, (char*)info->date, 1);
		add_next_index_string(return_value, (char*)info->author, 1);
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto mixed svn_mkdir(string path)
	Creates a directory in a working copy or repository
   */
PHP_FUNCTION(svn_mkdir)
{
	const char *path = NULL, *utf8_path = NULL;
	int pathlen;
	apr_pool_t *subpool;
	svn_error_t *err;
	svn_commit_info_t *info = NULL;
	apr_array_header_t *targets;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
					&path, &pathlen)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));

	if (!subpool) {
		RETURN_FALSE;
	}

	svn_utf_cstring_to_utf8 (&utf8_path, path, subpool);

	targets = apr_array_make (subpool, 1, sizeof(char *));

	APR_ARRAY_PUSH(targets, const char *) = svn_path_canonicalize(utf8_path, subpool);

	err = svn_client_mkdir2(&info, targets, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else if (info) {
		array_init(return_value);
		add_next_index_long(return_value, info->revision);
		add_next_index_string(return_value, (char*)info->date, 1);
		add_next_index_string(return_value, (char*)info->author, 1);
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto mixed svn_move(string src_path, string dst_path [,bool force])
	Move paths in a working copy or repository
   */
PHP_FUNCTION(svn_move)
{
	const char *src_path = NULL, *utf8_src_path = NULL;
	const char *dst_path = NULL, *utf8_dst_path = NULL;
	int src_pathlen, dst_pathlen;
	zend_bool force = 0;
	apr_pool_t *subpool;
	svn_error_t *err;
	svn_commit_info_t *info = NULL;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|b",
					&src_path, &src_pathlen, &dst_path, &dst_pathlen, &force)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));

	if (!subpool) {
		RETURN_FALSE;
	}

	svn_utf_cstring_to_utf8 (&utf8_src_path, src_path, subpool);
	svn_utf_cstring_to_utf8 (&utf8_dst_path, dst_path, subpool);

	utf8_src_path = svn_path_canonicalize(utf8_src_path, subpool);
	utf8_dst_path = svn_path_canonicalize(utf8_dst_path, subpool);

	err = svn_client_move3(&info, utf8_src_path, utf8_dst_path, force, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else if (info) {
		array_init(return_value);
		add_next_index_long(return_value, info->revision);
		add_next_index_string(return_value, (char*)info->date, 1);
		add_next_index_string(return_value, (char*)info->author, 1);
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto mixed svn_proplist(string path [,bool recurse])
	Move paths in a working copy or repository
   */
PHP_FUNCTION(svn_proplist)
{
	const char *path = NULL, *utf8_path = NULL;
	int pathlen;
	zend_bool recurse = 0;
	apr_pool_t *subpool;
	svn_error_t *err;
	apr_array_header_t *props;
	svn_opt_revision_t revision = { 0 }, peg_revision = { 0 };
	int i = 0;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b",
					&path, &pathlen, &recurse)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));

	if (!subpool) {
		RETURN_FALSE;
	}

	svn_utf_cstring_to_utf8 (&utf8_path, path, subpool);

	utf8_path = svn_path_canonicalize(utf8_path, subpool);

	err = svn_client_proplist2(&props, utf8_path, &peg_revision, &revision, recurse, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {

		array_init(return_value);

		for (i = 0; i < props->nelts; ++i) {
			svn_client_proplist_item_t *item
                = ((svn_client_proplist_item_t **)props->elts)[i];
			zval *row;
			apr_hash_index_t *hi;

			MAKE_STD_ZVAL(row);
			array_init(row);
			for (hi = apr_hash_first(subpool, item->prop_hash); hi; hi = apr_hash_next(hi)) {
				const void *key;
				void *val;
				const char *pname;
				svn_string_t *propval;

				apr_hash_this(hi, &key, NULL, &val);
				pname = key;
				propval = val;

				add_assoc_stringl(row, (char *)pname, (char *)propval->data, propval->len, 1);
			}
			add_assoc_zval(return_value, (char *)svn_path_local_style(item->node_name->data, subpool), row);
		}
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto mixed svn_propget(string path, string property_name [,bool recurse])
	Move paths in a working copy or repository
   */
PHP_FUNCTION(svn_propget)
{
	const char *path = NULL, *utf8_path = NULL;
	const char *propname = NULL;
	int pathlen, propnamelen;
	zend_bool recurse = 0;
	apr_pool_t *subpool;
	svn_error_t *err;
	apr_hash_t *props;
	svn_opt_revision_t revision = { 0 }, peg_revision = { 0 };
	apr_hash_index_t *hi;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|b",
					&path, &pathlen, &propname, &propnamelen, &recurse)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));

	if (!subpool) {
		RETURN_FALSE;
	}

	svn_utf_cstring_to_utf8 (&utf8_path, path, subpool);

	utf8_path = svn_path_canonicalize(utf8_path, subpool);

	err = svn_client_propget2(&props, propname, utf8_path, &peg_revision, &revision, recurse, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {

		array_init(return_value);
		for (hi = apr_hash_first(subpool, props); hi; hi = apr_hash_next(hi)) {
			const void *key;
			void *val;
			const char *pname;
			svn_string_t *propval;
			zval *row;

			MAKE_STD_ZVAL(row);
			array_init(row);
			apr_hash_this(hi, &key, NULL, &val);
			pname = key;
			propval = val;

			add_assoc_stringl(row, (char *)propname, (char *)propval->data, propval->len, 1);
			add_assoc_zval(return_value, (char *)svn_path_local_style(pname, subpool), row);
		}
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto resource svn_repos_create(string path [, array config [, array fsconfig]])
   Create a new subversion repository at path */
PHP_FUNCTION(svn_repos_create)
{
	char *path;
	int pathlen;
	zval *config = NULL;
	zval *fsconfig = NULL;
	apr_hash_t *config_hash = NULL;
	apr_hash_t *fsconfig_hash = NULL;
	apr_pool_t *subpool;
	svn_error_t *err;
	svn_repos_t *repos = NULL;
	struct php_svn_repos *resource = NULL;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a!a!",
				&path, &pathlen, &config, &fsconfig)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	config_hash = replicate_zend_hash_to_apr_hash(config, subpool TSRMLS_CC);
	fsconfig_hash = replicate_zend_hash_to_apr_hash(fsconfig, subpool TSRMLS_CC);

	err = svn_repos_create(&repos, path, NULL, NULL, config_hash, fsconfig_hash, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
	}

	if (repos) {
		resource = emalloc(sizeof(*resource));
		resource->pool = subpool;
		resource->repos = repos;
		ZEND_REGISTER_RESOURCE(return_value, resource, le_svn_repos);
	} else {
		svn_pool_destroy(subpool);
		RETURN_FALSE;
	}

}
/* }}} */

/* {{{ proto bool svn_repos_recover(string path)
   Run recovery procedures on the repository located at path. */
PHP_FUNCTION(svn_repos_recover)
{
	char *path;
	int pathlen;
	apr_pool_t *subpool;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
				&path, &pathlen)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_repos_recover2(path, 0, NULL, NULL, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto bool svn_repos_hotcopy(string repospath, string destpath, bool cleanlogs)
   Make a hot-copy of the repos at repospath; copy it to destpath */
PHP_FUNCTION(svn_repos_hotcopy)
{
	char *path, *dest;
	int pathlen, destlen;
	zend_bool cleanlogs;
	apr_pool_t *subpool;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssb",
				&path, &pathlen, &dest, &destlen, &cleanlogs)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_repos_hotcopy(path, dest, cleanlogs, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

static int replicate_array(void *pDest, int num_args, va_list args, zend_hash_key *key)
{
	zval **val = (zval **)pDest;
	apr_pool_t *pool = (apr_pool_t*)va_arg(args, apr_pool_t*);
	apr_array_header_t *arr = (apr_array_header_t*)va_arg(args, apr_array_header_t*);

	if (Z_TYPE_PP(val) == IS_STRING) {
		APR_ARRAY_PUSH(arr, const char*) = apr_pstrdup(pool, Z_STRVAL_PP(val));
	}

	va_end(args);

	return ZEND_HASH_APPLY_KEEP;
}


static apr_array_header_t *replicate_zend_hash_to_apr_array(zval *arr, apr_pool_t *pool TSRMLS_DC)
{
	apr_array_header_t *apr_arr = apr_array_make(pool, zend_hash_num_elements(Z_ARRVAL_P(arr)), sizeof(const char*));

	if (!apr_arr) return NULL;

	zend_hash_apply_with_arguments(Z_ARRVAL_P(arr), replicate_array, 2, pool, apr_arr);

	return apr_arr;
}

/* {{{ proto array svn_commit(string log, array targets [, bool dontrecurse])
   Sends changes from the local working copy to the repository */
PHP_FUNCTION(svn_commit)
{
	char *log, *path = NULL;
	int loglen, pathlen;
	zend_bool dontrecurse = 0;
	apr_pool_t *subpool;
	svn_error_t *err;
	svn_client_commit_info_t *info = NULL;
	zval *targets = NULL;
	apr_array_header_t *targets_array;

	if (FAILURE == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC, "ss|b",
				&log, &loglen, &path, &pathlen, &dontrecurse)) {
		if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|b",
					&log, &loglen, &targets, &dontrecurse)) {
			return;
		}
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	SVN_G(ctx)->log_msg_baton = log;

	if (path) {
		targets_array = apr_array_make (subpool, 1, sizeof(char *));
		APR_ARRAY_PUSH(targets_array, const char *) = path;
	} else {
		targets_array = replicate_zend_hash_to_apr_array(targets, subpool TSRMLS_CC);
	}

	err = svn_client_commit(&info, targets_array, dontrecurse, SVN_G(ctx), subpool);
	SVN_G(ctx)->log_msg_baton = NULL;

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else if (info) {
		array_init(return_value);
		add_next_index_long(return_value, info->revision);
		add_next_index_string(return_value, (char*)info->date, 1);
		add_next_index_string(return_value, (char*)info->author, 1);
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "commit didn't return any info");
		RETVAL_FALSE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto bool svn_add(string path [, bool recursive [, bool force]])
   Schedule the addition of a file in a working directory */
PHP_FUNCTION(svn_add)
{
	char *path;
	int pathlen;
	zend_bool recurse = 1, force = 0;
	apr_pool_t *subpool;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|bb",
				&path, &pathlen, &recurse, &force)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_client_add2((const char*)path, recurse, force, SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_TRUE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto array svn_status(string path [, int flags]])
   Returns the status of working copy files and directories */

static void php_svn_status_receiver(void *baton, const char *path, svn_wc_status2_t *status)
{
	zval *return_value = (zval*)baton;
	zval *entry;
	TSRMLS_FETCH();

	MAKE_STD_ZVAL(entry);
	array_init(entry);

	add_assoc_string(entry, "path", (char*)path, 1);
	if (status) {
		add_assoc_long(entry, "text_status", status->text_status);
		add_assoc_long(entry, "repos_text_status", status->repos_text_status);
		add_assoc_long(entry, "prop_status", status->prop_status);
		add_assoc_long(entry, "repos_prop_status", status->repos_prop_status);
		add_assoc_bool(entry, "locked", status->locked);
		add_assoc_bool(entry, "copied", status->copied);
		add_assoc_bool(entry, "switched", status->switched);

		if (status->entry) {
			if (status->entry->name) {
				add_assoc_string(entry, "name", (char*)status->entry->name, 1);
			}
			if (status->entry->url) {
				add_assoc_string(entry, "url", (char*)status->entry->url, 1);
			}
			if (status->entry->repos) {
				add_assoc_string(entry, "repos", (char*)status->entry->repos, 1);
			}

			add_assoc_long(entry, "revision", status->entry->revision);
			add_assoc_long(entry, "kind", status->entry->kind);
			add_assoc_long(entry, "schedule", status->entry->schedule);
			if (status->entry->deleted) add_assoc_bool(entry, "deleted", status->entry->deleted);
			if (status->entry->absent) add_assoc_bool(entry, "absent", status->entry->absent);
			if (status->entry->incomplete) add_assoc_bool(entry, "incomplete", status->entry->incomplete);

			if (status->entry->copyfrom_url) {
				add_assoc_string(entry, "copyfrom_url", (char*)status->entry->copyfrom_url, 1);
				add_assoc_long(entry, "copyfrom_rev", status->entry->copyfrom_rev);
			}

			if (status->entry->cmt_author) {
				add_assoc_long(entry, "cmt_date", apr_time_sec(status->entry->cmt_date));
				add_assoc_long(entry, "cmt_rev", status->entry->cmt_rev);
				add_assoc_string(entry, "cmt_author", (char*)status->entry->cmt_author, 1);
			}
			if (status->entry->prop_time) {
				add_assoc_long(entry, "prop_time", apr_time_sec(status->entry->prop_time));
			}

			if (status->entry->text_time) {
				add_assoc_long(entry, "text_time", apr_time_sec(status->entry->text_time));
			}
		}
	}

	add_next_index_zval(return_value, entry);
}

PHP_FUNCTION(svn_status)
{
	char *path;
	int path_len;
	long flags = 0;
	apr_pool_t *subpool;
	svn_error_t *err;
	svn_revnum_t result_revision;
	svn_opt_revision_t revision;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l",
				&path, &path_len, &flags)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	array_init(return_value);
	revision.kind = svn_opt_revision_head;

	err = svn_client_status2(
		&result_revision,
		path,
		&revision,
		php_svn_status_receiver,
		(void*)return_value,
		!(flags & SVN_NON_RECURSIVE),
		flags & SVN_ALL,
		flags & SVN_SHOW_UPDATES,
		flags & SVN_NO_IGNORE,
		flags & SVN_IGNORE_EXTERNALS,
		SVN_G(ctx),
		subpool);



	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto int svn_update(string path [, int revno [, bool recurse]])
   Update working copy at path to revno */
PHP_FUNCTION(svn_update)
{
	char *path;
	int pathlen;
	zend_bool recurse = 1;
	apr_pool_t *subpool;
	svn_error_t *err;
	svn_revnum_t result_rev;
	svn_opt_revision_t rev;
	long revno = -1;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lb",
				&path, &pathlen, &revno, &recurse)) {
		return;
	}

	PHP_SVN_INIT_CLIENT();
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	if (revno > 0) {
		rev.kind = svn_opt_revision_number;
		rev.value.number = revno;
	} else {
		rev.kind = svn_opt_revision_head;
	}
	err = svn_client_update(&result_rev, path, &rev, recurse,
			SVN_G(ctx), subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	} else {
		RETVAL_LONG(result_rev);
	}

	svn_pool_destroy(subpool);
}
/* }}} */


static void php_svn_get_version(char *buf, int buflen)
{
	const svn_version_t *vers;
	vers = svn_client_version();

	if (strlen(vers->tag))
		snprintf(buf, buflen, "%d.%d.%d#%s", vers->major, vers->minor, vers->patch, vers->tag);
	else
		snprintf(buf, buflen, "%d.%d.%d", vers->major, vers->minor, vers->patch);
}

/* {{{ proto string svn_client_version()
   Returns the version of the SVN client libraries */
PHP_FUNCTION(svn_client_version)
{
	char vers[128];

	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	php_svn_get_version(vers, sizeof(vers));
	RETURN_STRING(vers, 1);
}
/* }}} */

/* {{{ proto resource svn_repos_fs_begin_txn_for_commit(resource repos, long rev, string author, string log_msg)
   create a new transaction */
PHP_FUNCTION(svn_repos_fs_begin_txn_for_commit)
{
	svn_fs_txn_t *txn_p = NULL;
	struct php_svn_repos_fs_txn *new_txn = NULL;
	zval *zrepos;
	struct php_svn_repos *repos = NULL;
	svn_revnum_t rev;
	char *author;
	int author_len;
	char *log_msg;
	int log_msg_len;
	apr_pool_t *subpool;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rlss",
				&zrepos, &rev, &author, &author_len, &log_msg, &log_msg_len)) {
		return;
	}

	ZEND_FETCH_RESOURCE(repos, struct php_svn_repos *, &zrepos, -1, "svn-repos", le_svn_repos);

	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_repos_fs_begin_txn_for_commit(&txn_p, repos->repos, rev, author, log_msg, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
	}

	if (txn_p) {
		new_txn = emalloc(sizeof(*new_txn));
		new_txn->repos = repos;
		zend_list_addref(repos->rsrc_id);
		new_txn->txn = txn_p;

		ZEND_REGISTER_RESOURCE(return_value, new_txn, le_svn_repos_fs_txn);
	} else {
		// something went wrong
	}
}
/* }}} */

/* {{{ proto int svn_repos_fs_commit_txn(resource txn)
   commits a transaction and returns the new revision */
PHP_FUNCTION(svn_repos_fs_commit_txn)
{
	zval *ztxn;
	struct php_svn_repos_fs_txn *txn;
	const char *conflicts;
	svn_revnum_t new_rev;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
				&ztxn)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(txn, struct php_svn_repos_fs_txn *, &ztxn, -1, "svn-repos-fs-txn", le_svn_repos_fs_txn);

	err = svn_repos_fs_commit_txn(&conflicts, txn->repos->repos, &new_rev, txn->txn, txn->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	}

	RETURN_LONG(new_rev);
}
/* }}} */

/* {{{ proto resource svn_fs_txn_root(resource txn)
   creates and returns a transaction root */
PHP_FUNCTION(svn_fs_txn_root)
{
	svn_fs_root_t *root_p = NULL;
	struct php_svn_fs_root *new_root = NULL;
	zval *ztxn;
	struct php_svn_repos_fs_txn *txn = NULL;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
				&ztxn)) {
		return;
	}

	ZEND_FETCH_RESOURCE(txn, struct php_svn_repos_fs_txn *, &ztxn, -1, "svn-fs-repos-txn", le_svn_repos_fs_txn);

	err = svn_fs_txn_root(&root_p, txn->txn, txn->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
	}

	if (root_p) {
		new_root = emalloc(sizeof(*new_root));
		new_root->repos = txn->repos;
		zend_list_addref(txn->repos->rsrc_id);
		new_root->root = root_p;

		ZEND_REGISTER_RESOURCE(return_value, new_root, le_svn_fs_root);
	} else {
		// something went wrong
	}
}
/* }}} */

/* {{{ proto bool svn_fs_make_file(resource root, string path)
   creates a new empty file, returns true if all is ok, false otherwise */
PHP_FUNCTION(svn_fs_make_file)
{
	zval *zroot;
	struct php_svn_fs_root *root = NULL;
	char *path;
	int path_len;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zroot, &path, &path_len)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(root, struct php_svn_fs_root *, &zroot, -1, "svn-fs-root", le_svn_fs_root);

	err = svn_fs_make_file(root->root, path, root->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ proto bool svn_fs_make_dir(resource root, string path)
   creates a new empty directory, returns true if all is ok, false otherwise*/
PHP_FUNCTION(svn_fs_make_dir)
{
	zval *zroot;
	struct php_svn_fs_root *root = NULL;
	char *path;
	int path_len;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zroot, &path, &path_len)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(root, struct php_svn_fs_root *, &zroot, -1, "svn-fs-root", le_svn_fs_root);

	err = svn_fs_make_dir(root->root, path, root->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
    } else {
		RETURN_TRUE;
	}
}
/* }}} */


/* {{{ proto resource svn_fs_apply_text(resource root, string path)
   creates and returns a stream that will be used to replace
   the content of an existing file*/
PHP_FUNCTION(svn_fs_apply_text)
{
	zval *zroot;
	struct php_svn_fs_root *root = NULL;
	char *path;
	int path_len;
	svn_error_t *err;
	svn_stream_t *stream_p = NULL;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zroot, &path, &path_len)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(root, struct php_svn_fs_root *, &zroot, -1, "svn-fs-root", le_svn_fs_root);

	err = svn_fs_apply_text(&stream_p, root->root, path, NULL, root->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	}

	if (stream_p) {
		php_stream *stm;
		stm = php_stream_alloc(&php_svn_stream_ops, stream_p, 0, "w");
		php_stream_to_zval(stm, return_value);
	} else {
		// something went wrong
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto bool svn_fs_copy(resource from_root, string from_path, resourse to_root, string to_path)
   copies a file or a directory, returns true if all is ok, false otherwise */
PHP_FUNCTION(svn_fs_copy)
{
	zval *zfrom_root, *zto_root;
	struct php_svn_fs_root *from_root, *to_root;
	char *from_path, *to_path;
	int from_path_len, to_path_len;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsrs",
				&zfrom_root, &from_path, &from_path_len,
				&zto_root, &to_path, &to_path_len)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(from_root, struct php_svn_fs_root *, &zfrom_root, -1, "svn-fs-root", le_svn_fs_root);
	ZEND_FETCH_RESOURCE(to_root, struct php_svn_fs_root *, &zto_root, -1, "svn-fs-root", le_svn_fs_root);

	err = svn_fs_copy(from_root->root, from_path, to_root->root, to_path, to_root->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ proto bool svn_fs_delete(resource root, string path)
   deletes a file or a directory, return true if all is ok, false otherwise */
PHP_FUNCTION(svn_fs_delete)
{
	zval *zroot;
	struct php_svn_fs_root *root = NULL;
	char *path;
	int path_len;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zroot, &path, &path_len)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(root, struct php_svn_fs_root *, &zroot, -1, "svn-fs-root", le_svn_fs_root);

	err = svn_fs_delete(root->root, path, root->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ proto resource svn_fs_begin_txn2(resource repos, long rev)
   create a new transaction */
PHP_FUNCTION(svn_fs_begin_txn2)
{
	svn_fs_txn_t *txn_p = NULL;
	struct php_svn_repos_fs_txn *new_txn = NULL;
	zval *zfs;
	struct php_svn_fs *fs = NULL;
	svn_revnum_t rev;
	apr_pool_t *subpool;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl",
				&zfs, &rev)) {
		return;
	}

	ZEND_FETCH_RESOURCE(fs, struct php_svn_fs *, &zfs, -1, "svn-fs", le_svn_fs);

	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	err = svn_fs_begin_txn2(&txn_p, fs->fs, rev, 0, subpool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
	}

	if (txn_p) {
		new_txn = emalloc(sizeof(*new_txn));
		new_txn->repos = fs->repos;
		zend_list_addref(fs->repos->rsrc_id);
		new_txn->txn = txn_p;

		ZEND_REGISTER_RESOURCE(return_value, new_txn, le_svn_repos_fs_txn);
	} else {
		// something went wrong
	}
}
/* }}} */

/* {{{ proto bool svn_fs_is_file(resource root, string path)
   return true if the path points to a file, false otherwise */
PHP_FUNCTION(svn_fs_is_file)
{
	zval *zroot;
	struct php_svn_fs_root *root = NULL;
	char *path;
	int path_len;
	svn_error_t *err;
	int is_file;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zroot, &path, &path_len)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(root, struct php_svn_fs_root *, &zroot, -1, "svn-fs-root", le_svn_fs_root);

	err = svn_fs_is_file(&is_file, root->root, path, root->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	} else {
		RETURN_BOOL(is_file);
	}
}
/* }}} */

/* {{{ proto bool svn_fs_is_dir(resource root, string path)
   return true if the path points to a directory, false otherwise */
PHP_FUNCTION(svn_fs_is_dir)
{
	zval *zroot;
	struct php_svn_fs_root *root = NULL;
	char *path;
	int path_len;
	svn_error_t *err;
	int is_dir;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs",
				&zroot, &path, &path_len)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(root, struct php_svn_fs_root *, &zroot, -1, "svn-fs-root", le_svn_fs_root);

	err = svn_fs_is_dir(&is_dir, root->root, path, root->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	} else {
		RETURN_BOOL(is_dir);
	}
}
/* }}} */

/* {{{ proto bool svn_fs_change_node_prop(resource root, string path, string name, string value)
   return true if everything is ok, false otherwise */
PHP_FUNCTION(svn_fs_change_node_prop)
{
	zval *zroot;
	struct php_svn_fs_root *root = NULL;
	char *path;
	int path_len;
	char *name;
	int name_len;
	char *value;
	int value_len;
	svn_string_t *svn_value;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsss",
				&zroot, &path, &path_len, &name, &name_len,
				&value, &value_len)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(root, struct php_svn_fs_root *, &zroot, -1, "svn-fs-root", le_svn_fs_root);

	svn_value = emalloc(sizeof(*svn_value));
	svn_value->data = value;
	svn_value->len = value_len;

	err = svn_fs_change_node_prop(root->root, path, name, svn_value,
			root->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ proto bool svn_fs_contents_changed(resource root1, string path1, resource root2, string path2)
   return true if content is different, false otherwise */
PHP_FUNCTION(svn_fs_contents_changed)
{
	zval *zroot1;
	struct php_svn_fs_root *root1 = NULL;
	zval *zroot2;
	struct php_svn_fs_root *root2 = NULL;
	char *path1;
	int path1_len;
	char *path2;
	int path2_len;
	svn_boolean_t changed;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsrs",
				&zroot1, &path1, &path1_len,
				&zroot2, &path2, &path2_len)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(root1, struct php_svn_fs_root *, &zroot1, -1, "svn-fs-root", le_svn_fs_root);
	ZEND_FETCH_RESOURCE(root2, struct php_svn_fs_root *, &zroot2, -1, "svn-fs-root", le_svn_fs_root);

	err = svn_fs_contents_changed(&changed, root1->root, path1,
			root2->root, path2,	root1->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	} else {
		if (changed == 1) {
			RETURN_TRUE;
		} else {
			RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto bool svn_fs_props_changed(resource root1, string path1, resource root2, string path2)
   return true if props are different, false otherwise */
PHP_FUNCTION(svn_fs_props_changed)
{
	zval *zroot1;
	struct php_svn_fs_root *root1 = NULL;
	zval *zroot2;
	struct php_svn_fs_root *root2 = NULL;
	char *path1;
	int path1_len;
	char *path2;
	int path2_len;
	svn_boolean_t changed;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsrs",
				&zroot1, &path1, &path1_len,
				&zroot2, &path2, &path2_len)) {
		RETURN_FALSE;
	}

	ZEND_FETCH_RESOURCE(root1, struct php_svn_fs_root *, &zroot1, -1, "svn-fs-root", le_svn_fs_root);
	ZEND_FETCH_RESOURCE(root2, struct php_svn_fs_root *, &zroot2, -1, "svn-fs-root", le_svn_fs_root);

	err = svn_fs_props_changed(&changed, root1->root, path1,
			root2->root, path2,	root1->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	} else {
		if (changed == 1) {
			RETURN_TRUE;
		} else {
			RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto bool svn_fs_abort_txn(resource txn)
   abort a transaction, returns true if everything is ok, false othewise */
PHP_FUNCTION(svn_fs_abort_txn)
{
	zval *ztxn;
	struct php_svn_repos_fs_txn *txn;
	svn_error_t *err;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r",
				&ztxn)) {
		return;
	}

	ZEND_FETCH_RESOURCE(txn, struct php_svn_repos_fs_txn *, &ztxn, -1, "svn-repos-fs-txn", le_svn_repos_fs_txn);

	err = svn_fs_abort_txn(txn->txn, txn->repos->pool);

	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETURN_FALSE;
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

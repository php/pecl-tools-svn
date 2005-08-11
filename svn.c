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

/* If you declare any globals in php_svn.h uncomment this: */
ZEND_DECLARE_MODULE_GLOBALS(svn)

/* custom property for ignoring SSL cert verification errors */
#define PHP_SVN_AUTH_PARAM_IGNORE_SSL_VERIFY_ERRORS "php:svn:auth:ignore-ssl-verify-errors"
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

static int le_svn_repos;
static int le_svn_fs;
static int le_svn_fs_root;


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
	PHP_FE(svn_commit, NULL)
	PHP_FE(svn_add, NULL)
	PHP_FE(svn_status, NULL)
	PHP_FE(svn_update, NULL)
	PHP_FE(svn_repos_create, NULL)
	PHP_FE(svn_repos_recover, NULL)
	PHP_FE(svn_repos_hotcopy, NULL)
	PHP_FE(svn_repos_open, NULL)
	PHP_FE(svn_repos_fs, NULL)
	PHP_FE(svn_fs_revision_root, NULL)
	PHP_FE(svn_fs_check_path, NULL)
	PHP_FE(svn_fs_revision_prop, NULL)
	PHP_FE(svn_fs_dir_entries, NULL)
	PHP_FE(svn_fs_node_created_rev, NULL)
	PHP_FE(svn_fs_node_prop, NULL)
	PHP_FE(svn_fs_youngest_rev, NULL)
	PHP_FE(svn_fs_file_contents, NULL)
	PHP_FE(svn_fs_file_length, NULL)
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
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_SVN
ZEND_GET_MODULE(svn)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("svn.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_svn_globals, svn_globals)
    STD_PHP_INI_ENTRY("svn.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_svn_globals, svn_globals)
PHP_INI_END()
*/
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

static void init_svn_client(TSRMLS_D)
{
	svn_error_t *err;
	svn_boolean_t store_password_val = TRUE;
	svn_auth_provider_object_t *provider;
	svn_auth_baton_t *ab;
	apr_array_header_t *providers;

	if (SVN_G(pool)) return;

	SVN_G(pool) = svn_pool_create(NULL);

	if ((err = svn_client_create_context (&SVN_G(ctx), SVN_G(pool)))) {
		php_svn_handle_error(err TSRMLS_CC);
		return;
	}

	if ((err = svn_config_get_config(&SVN_G(ctx)->config, NULL, SVN_G(pool)))) {
		php_svn_handle_error(err TSRMLS_CC);
		return;
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
}

PHP_FUNCTION(svn_auth_get_parameter)
{
	char *key;
	int keylen;
	const char *value;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &keylen)) {
		return;
	}

	init_svn_client(TSRMLS_C);

	value = svn_auth_get_parameter(SVN_G(ctx)->auth_baton, key);
	if (value) {
		RETURN_STRING((char*)value, 1);
	}
}

PHP_FUNCTION(svn_auth_set_parameter)
{
	char *key, *value;
	int keylen, valuelen;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &keylen, &value, &valuelen)) {
		return;
	}
	init_svn_client(TSRMLS_C);

	svn_auth_set_parameter(SVN_G(ctx)->auth_baton, apr_pstrdup(SVN_G(pool), key), apr_pstrdup(SVN_G(pool), value));
}

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(svn)
{
	apr_initialize();
	ZEND_INIT_MODULE_GLOBALS(svn, php_svn_init_globals, NULL);

#define STRING_CONST(foo) REGISTER_STRING_CONSTANT(#foo, foo, CONST_CS|CONST_PERSISTENT)
#define LONG_CONST(foo) REGISTER_LONG_CONSTANT(#foo, foo, CONST_CS|CONST_PERSISTENT)
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
	
	/* this is probably temporary until we sort out a proper revision parser. */
	REGISTER_LONG_CONSTANT("SVN_REVISON_HEAD", -1, CONST_CS|CONST_PERSISTENT);


	le_svn_repos = zend_register_list_destructors_ex(php_svn_repos_dtor,
			NULL, "svn-repos", module_number);

	le_svn_fs = zend_register_list_destructors_ex(php_svn_fs_dtor,
			NULL, "svn-fs", module_number);

	le_svn_fs_root = zend_register_list_destructors_ex(php_svn_fs_root_dtor,
			NULL, "svn-fs-root", module_number);
		
 
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
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* reference http://www.linuxdevcenter.com/pub/a/linux/2003/04/24/libsvn1.html */

/* {{{ proto bool svn_checkout(string repos, string targetpath [, int revision])
   Checks out a particular revision from repos into targetpath */
PHP_FUNCTION(svn_checkout)
{
	char *repos_url = NULL, *target_path = NULL;
	int repos_url_len, target_path_len;
	svn_error_t *err;
	svn_opt_revision_t revision = { 0 };
	long revno = -1;
	apr_pool_t *subpool;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|l", 
			&repos_url, &repos_url_len, &target_path, &target_path_len, &revno) == FAILURE) {
		return;
	}
	
	init_svn_client(TSRMLS_C);
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}
	
	if (revno > 0) {
		revision.kind = svn_opt_revision_number;
		revision.value.number = revno;
	} else {
		revision.kind = svn_opt_revision_head;
	}
	
	err = svn_client_checkout (NULL,
			repos_url,
			target_path,
			&revision,
			TRUE, /* yes, we want to recurse into the URL */
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


PHP_FUNCTION(svn_cat)
{
	char *repos_url = NULL;
	int repos_url_len, revision_no = -1, size;
	svn_error_t *err;
	svn_opt_revision_t revision = { 0 };
	svn_stream_t *out = NULL;
	svn_stringbuf_t *buf = NULL;
	char *retdata =NULL;
	apr_pool_t *subpool;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", 
		&repos_url, &repos_url_len, &revision_no) == FAILURE) {
		return;
	}
	init_svn_client(TSRMLS_C);
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

	err = svn_client_cat(out, repos_url, &revision, SVN_G(ctx), subpool);

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


PHP_FUNCTION(svn_ls)
{
	char *repos_url = NULL;
	int repos_url_len, size, revision_no = -1;
	svn_error_t *err;
	svn_opt_revision_t revision = { 0 };
	svn_stream_t *out;
	svn_stringbuf_t *buf;
	char *retdata =NULL;
	apr_hash_t *dirents;
	apr_array_header_t *array;
	int i;
	apr_pool_t *subpool;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", 
			&repos_url, &repos_url_len, &revision_no) == FAILURE) {
		return;
	}
	init_svn_client(TSRMLS_C);
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

static svn_error_t *
php_svn_log_message_receiver (	void *baton,
				apr_hash_t *changed_paths,
				svn_revnum_t rev,
				const char *author,
				const char *date,
				const char *msg,
				apr_pool_t *pool)
{
	zval *return_value = (zval *)baton, *row, *paths;
	char *path;
	apr_hash_index_t *hi;
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
	if (msg) {
		add_assoc_string(row, "msg", (char *) msg, 1);
	}
	if (date) {
		add_assoc_string(row, "date", (char *) date, 1);
	}

	if (!changed_paths) {
		add_next_index_zval(return_value, row); 
		return SVN_NO_ERROR;
	}

	MAKE_STD_ZVAL(paths);
	array_init(paths);

	sorted_paths = svn_sort__hash(changed_paths,
			svn_sort_compare_items_as_paths, pool);

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
	add_next_index_zval(return_value, row); 
	return SVN_NO_ERROR;
}
 
PHP_FUNCTION(svn_log)
{
	char *repos_url = NULL, *utf8_repos_url = NULL; 
	int repos_url_len;
	int revision = -2;
	svn_error_t *err;
	svn_opt_revision_t 	start_revision = { 0 }, end_revision = { 0 };
	char *retdata =NULL;
	int size;
	apr_array_header_t *targets;
	const char *target;
	int i;
	apr_pool_t *subpool;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", 
			&repos_url, &repos_url_len, &revision) == FAILURE) {
		return;
	}
	init_svn_client(TSRMLS_C);
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}
	RETVAL_FALSE;
  
	svn_utf_cstring_to_utf8 (&utf8_repos_url, repos_url, subpool);
	if (revision < -1) {
		start_revision.kind =  svn_opt_revision_head;
		end_revision.kind   =  svn_opt_revision_number;
		end_revision.value.number = 1 ;
	} else  if (revision == -1) {
		start_revision.kind =  svn_opt_revision_head;
		end_revision.kind   =  svn_opt_revision_head;
	} else {						
		start_revision.kind =  svn_opt_revision_number;
		start_revision.value.number = revision ;
		end_revision.kind   =  svn_opt_revision_number;
		end_revision.value.number = revision ;
	}
	 
	
	targets = apr_array_make (subpool, 1, sizeof(char *));
	
	APR_ARRAY_PUSH(targets, const char *) = 
		svn_path_canonicalize(utf8_repos_url, subpool);
	array_init(return_value);
	
	err = svn_client_log(
		targets,
		&start_revision,
		&end_revision,
		1, // svn_boolean_t discover_changed_paths, 
		1, // svn_boolean_t strict_node_history, 
		php_svn_log_message_receiver,
		(void *) return_value,
		SVN_G(ctx), subpool);
 
	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	}

	svn_pool_destroy(subpool);
}

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

/* {{{ proto mixed svn_diff(string path1, int rev1, string path2, int rev2)
   Recursively diffs two paths.  Returns an array consisting of two streams: the first is the diff output and the second contains error stream output */
PHP_FUNCTION(svn_diff)
{
	const char *tmp_dir;
	char outname[256], errname[256];
	apr_pool_t *subpool;
	apr_file_t *outfile = NULL, *errfile = NULL;
	svn_error_t *err;
	char *path1, *path2;
	int path1len, path2len;
	long rev1 = -1, rev2 = -1;
	apr_array_header_t diff_options = { 0, 0, 0, 0, 0};
	svn_opt_revision_t revision1, revision2;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl!sl!",
			&path1, &path1len, &rev1,
			&path2, &path2len, &rev2)) {
		return;
	}

	init_svn_client(TSRMLS_C);
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

	err = svn_client_diff(&diff_options,
			path1, &revision1,
			path2, &revision2,
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

	init_svn_client(TSRMLS_C);
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

/* {{{ proto resource svn_fs_youngest_rev(resource fs)
   Returns the number of the youngest revision in the filesystem */
PHP_FUNCTION(svn_fs_youngest_rev)
{
	zval *zfs;
	struct php_svn_fs *fs;
	svn_fs_root_t *root;
	svn_error_t *err;
	struct php_svn_fs_root *resource;
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
	svn_filesize_t len;
	svn_error_t *err;
	apr_pool_t *subpool;
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

/* {{{ proto long svn_fs_node_prop(resource fsroot, string path, string propname)
   Returns the value of a property for a node */
PHP_FUNCTION(svn_fs_node_prop)
{
	zval *zfsroot;
	struct php_svn_fs_root *fsroot;
	char *path, *propname;
	int pathlen, propnamelen;
	svn_error_t *err;
	apr_pool_t *subpool;
	svn_revnum_t rev;
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
		RETVAL_STRINGL((char*)val->data, val->len, 1);
	}
	svn_pool_destroy(subpool);
}
/* }}} */


/* {{{ proto long svn_fs_node_created_rev(resource fsroot, string path)
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

	init_svn_client(TSRMLS_C);
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

	init_svn_client(TSRMLS_C);
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

	init_svn_client(TSRMLS_C);
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

	init_svn_client(TSRMLS_C);
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
   Make a hot-copy of the repos at repospath; copy it to destpath */
PHP_FUNCTION(svn_commit)
{
	char *log;
	int loglen;
	zend_bool dontrecurse = 0;
	apr_pool_t *subpool;
	svn_error_t *err;
	svn_client_commit_info_t *info = NULL;
	zval *targets = NULL;
	apr_array_header_t *targets_array;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|b",
				&log, &loglen, &targets, &dontrecurse)) {
		return;
	}

	init_svn_client(TSRMLS_C);
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	SVN_G(ctx)->log_msg_baton = log;

	targets_array = replicate_zend_hash_to_apr_array(targets, subpool TSRMLS_CC);
	
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

/* {{{ proto array svn_add(string path [, bool recursive [, bool force]])
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

	init_svn_client(TSRMLS_C);
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

/* {{{ proto array svn_status(string path [, bool recursive [, bool get_all [, bool update [, bool no_ignore]]]])
   Schedule the addition of a file in a working directory */

static void status_func(void *baton, const char *path, svn_wc_status_t *status)
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
		if (status->locked) add_assoc_bool(entry, "locked", status->locked);
		if (status->copied) add_assoc_bool(entry, "copied", status->copied);
		if (status->switched) add_assoc_bool(entry, "switched", status->switched);

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
	int pathlen;
	zend_bool recurse = 1, get_all = 0, update = 0, no_ignore = 0;
	apr_pool_t *subpool;
	svn_error_t *err;
	svn_revnum_t result_rev;
	svn_opt_revision_t rev;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|bbbb",
				&path, &pathlen, &recurse, &get_all, &update, &no_ignore)) {
		return;
	}

	init_svn_client(TSRMLS_C);
	subpool = svn_pool_create(SVN_G(pool));
	if (!subpool) {
		RETURN_FALSE;
	}

	array_init(return_value);
	rev.kind = svn_opt_revision_head;
	err = svn_client_status(&result_rev, path, &rev, status_func, return_value,
			recurse, get_all, update, no_ignore, SVN_G(ctx), subpool);
	
	if (err) {
		php_svn_handle_error(err TSRMLS_CC);
		RETVAL_FALSE;
	}

	svn_pool_destroy(subpool);
}
/* }}} */

/* {{{ proto array svn_update(string path [, int revno [, bool recurse]])
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

	init_svn_client(TSRMLS_C);
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

PHP_FUNCTION(svn_client_version)
{
	char vers[128];

	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	php_svn_get_version(vers, sizeof(vers));
	RETURN_STRING(vers, 1);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2004 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Alan Knowles <alan@akbkhome.com>                             |
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

/* If you declare any globals in php_svn.h uncomment this: */
ZEND_DECLARE_MODULE_GLOBALS(svn)


/* True global resources - no need for thread safety here */
static int le_svn;

/* {{{ svn_functions[]
 *
 * Every user visible function must have an entry in svn_functions[].
 */
function_entry svn_functions[] = {
	PHP_FE(confirm_svn_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE(svn_checkout,		NULL)
	PHP_FE(svn_cat,			NULL)
	PHP_FE(svn_ls,			NULL)
	PHP_FE(svn_log,			NULL)
	{NULL, NULL, NULL}	/* Must be the last line in svn_functions[] */
};
/* }}} */

/* {{{ svn_module_entry
 */
zend_module_entry svn_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"svn",
	svn_functions,
	PHP_MINIT(svn),
	PHP_MSHUTDOWN(svn),
	PHP_RINIT(svn),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(svn),	/* Replace with NULL if there's nothing to do at request end */
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

/* {{{ php_svn_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_svn_init_globals(zend_svn_globals *svn_globals)
{
	svn_globals->global_value = 0;
	svn_globals->global_string = NULL;
}
*/
/* }}} */

int 
set_up_client_ctx ()
{
	
	
	
	
	 
	svn_error_t *err;
	apr_initialize();
	
	apr_pool_create(&SVN_G(pool), NULL);
	
	
	
	if ((err = svn_client_create_context (&SVN_G(ctx), SVN_G(pool)))) {
		svn_handle_error (err, stdout, TRUE);
		return 1;
	}
	 
	if ((err = svn_config_get_config (&(SVN_G(ctx)->config),
                                    NULL, SVN_G(pool)))) {
		svn_handle_error (err, stdout, TRUE);
		return 1;
	}
	 
	{	/* authentication */
		svn_boolean_t store_password_val = TRUE;
		svn_auth_provider_object_t *provider;
		svn_auth_baton_t *ab;
		
		/* The whole list of registered providers */
		apr_array_header_t *providers
			= apr_array_make (SVN_G(pool), 10, sizeof (svn_auth_provider_object_t *));
		
		/* The main disk-caching auth providers, for both
		'username/password' creds and 'username' creds.  */
		svn_client_get_simple_provider (&provider, SVN_G(pool));
		APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;
		svn_client_get_username_provider (&provider, SVN_G(pool));
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
	return 0;
}


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(svn)
{
	/* If you have INI entries, uncomment these lines 
	ZEND_INIT_MODULE_GLOBALS(svn, php_svn_init_globals, NULL);
	REGISTER_INI_ENTRIES();
	*/
	set_up_client_ctx();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(svn)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(svn)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(svn)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(svn)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "svn support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_svn_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_svn_compiled)
{
	char *arg = NULL;
	int arg_len, len;
	char string[256];

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	len = sprintf(string, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "svn", arg);
	RETURN_STRINGL(string, len, 1);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/






/* reference http://www.linuxdevcenter.com/pub/a/linux/2003/04/24/libsvn1.html */

PHP_FUNCTION(svn_checkout)
{
	char 	*repos_url = NULL,
		*target_path = NULL;
	int 	repos_url_len,
		target_path_len;
	svn_error_t *err;
	svn_opt_revision_t revision = { 0 };

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", 
		&repos_url, &repos_url_len, &target_path, &target_path_len) == FAILURE) {
		return;
	}
	
	
	
	/* grab the most recent version of the website. */
	revision.kind = svn_opt_revision_head;
	
	err = svn_client_checkout (NULL,
				    repos_url,
				     target_path,
				     &revision,
				     TRUE, /* yes, we want to recurse into the URL */
				     SVN_G(ctx),
				     SVN_G(pool));
	if (err) {
		svn_handle_error (err, stdout, TRUE);
	} else {
		/* printf ("deployment succeeded.\n"); */
	}
}


PHP_FUNCTION(svn_cat)
{
	char 	*repos_url = NULL;
		 
	int 	repos_url_len,
		revision_no = -1,
		size;
	svn_error_t *err;
	svn_opt_revision_t revision = { 0 };
	svn_stream_t *out;
	svn_stringbuf_t *buf;
	char *retdata =NULL;

	
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", 
		&repos_url, &repos_url_len, &revision_no) == FAILURE) {
		return;
	}
	
	
	svn_stream_for_stdout (&out, SVN_G(pool));
	
	
	if (revision_no == -1) {
		revision.kind = svn_opt_revision_head;
	} else {
		revision.kind =   svn_opt_revision_number;
		revision.value.number = revision_no ;
	}
	
	buf = svn_stringbuf_create("", SVN_G(pool));
	out = svn_stream_from_stringbuf(buf, SVN_G(pool));
	err = svn_client_cat (out, 
				repos_url, 
				&revision,
                               SVN_G(ctx), SVN_G(pool));
	if (err) {
		svn_handle_error (err, stdout, TRUE);
	} else {
		/* printf ("deployment succeeded.\n"); */
	}
	retdata = emalloc(buf->len);
	err = svn_stream_read (out, (char *)retdata, &size);
	if (err) {
		svn_handle_error (err, stdout, TRUE);
	} else {
		/* printf ("deployment succeeded.\n"); */
	}
	RETURN_STRINGL(retdata, size,0);
	
}


PHP_FUNCTION(svn_ls)
{
	char 	*repos_url = NULL;
		 
	int 	repos_url_len,
		size,
		revision_no = -1;
	svn_error_t *err;
	svn_opt_revision_t revision = { 0 };
	svn_stream_t *out;
	svn_stringbuf_t *buf;
	char *retdata =NULL;
	
	 
	apr_hash_t *dirents;
	apr_array_header_t *array;
	int i;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", 
		&repos_url, &repos_url_len, &revision_no) == FAILURE) {
		return;
	}
	 
	if (revision_no == -1) {
		revision.kind = svn_opt_revision_head;
	} else {
		revision.kind =   svn_opt_revision_number;
		revision.value.number = revision_no ;
	}
	/* grab the most recent version of the website. */
	
	 
	 
	
	err = svn_client_ls (&dirents,
                repos_url, 
                &revision,
                FALSE,
                SVN_G(ctx), SVN_G(pool));
  
	 
	if (err) {
		svn_handle_error (err, stdout, TRUE);
	} else {
		/* printf ("deployment succeeded.\n"); */
	}
	
	array = svn_sort__hash (dirents, svn_sort_compare_items_as_paths,SVN_G(pool));
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
				      "%b %d  %Y", &exp_time);
		}
		
		/* if that failed, just zero out the string and print nothing */
		if (apr_err)
			timestr[0] = '\0';
		
		/* we need it in UTF-8. */
		svn_utf_cstring_to_utf8 (&utf8_timestr, timestr,  SVN_G(pool));
		
		 
		MAKE_STD_ZVAL(row);
		array_init(row);
		add_assoc_long(row, "created", (long) dirent->created_rev);
		add_assoc_string(row, "modified_by", dirent->last_author ? (char *) dirent->last_author : " ? ", 1);
		add_assoc_long(row, "size", dirent->size);
		add_assoc_string(row, "modified", timestr,1);
		add_assoc_string(row, "name", (char *) utf8_entryname,1);
		add_assoc_string(row, "type", (dirent->kind == svn_node_dir) ? "dir" : "file",1);
		add_next_index_zval(return_value,row); 
	}
	
	
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
	zval 	*return_value =  (zval *)baton,
		*row,
		*paths;
	char 	*path;
	apr_hash_index_t *hi;
	apr_array_header_t *sorted_paths;
	int i;
	TSRMLS_FETCH();
	
	if (rev == 0)
		return SVN_NO_ERROR;
	 
	
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
		add_next_index_zval(return_value,row); 
		return SVN_NO_ERROR;
	}
	
	MAKE_STD_ZVAL(paths);
	array_init(paths);
	 
	sorted_paths = svn_sort__hash (changed_paths,
                                      svn_sort_compare_items_as_paths, SVN_G(pool));
  
            
	for (i = 0; i < sorted_paths->nelts; i++)
	{
	       svn_sort__item_t *item;
	       svn_log_changed_path_t *log_item;
	       zval *zpaths;
	       
		MAKE_STD_ZVAL(zpaths);
		array_init(zpaths);
		item = &(APR_ARRAY_IDX (sorted_paths, i,
                                                     svn_sort__item_t));
		const char *path = item->key;
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
	add_next_index_zval(return_value,row); 
	return SVN_NO_ERROR;
}
 

PHP_FUNCTION(svn_log)
{
	char 	*repos_url 	= NULL,
		*utf8_repos_url = NULL; 
	int 	repos_url_len;
	int	revision = -1;
		 
	svn_error_t *err;
	svn_opt_revision_t 	start_revision = { 0 },
				end_revision = { 0 };
	
	char *retdata =NULL;
	int size;
	apr_array_header_t *targets;
	
	const char *target;
	int i;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", 
		&repos_url, &repos_url_len, &revision) == FAILURE) {
		return;
	}
	 
	svn_utf_cstring_to_utf8 (&utf8_repos_url, repos_url,  SVN_G(pool));
	if (revision == -1) {
		start_revision.kind =  svn_opt_revision_head;
		end_revision.kind   =  svn_opt_revision_number;
		end_revision.value.number = 1 ;
	} else {
		start_revision.kind =  svn_opt_revision_number;
		start_revision.value.number = revision ;
		end_revision.kind   =  svn_opt_revision_number;
		end_revision.value.number = revision ;
	}
	
	targets = apr_array_make (SVN_G(pool), 1, sizeof(char *));
	
	APR_ARRAY_PUSH(targets, const char *) = 
		svn_path_canonicalize(utf8_repos_url, SVN_G(pool));
	//MAKE_STD_ZVAL(return_value);
	array_init(return_value);
	
	err = svn_client_log(
		targets,
		&start_revision,
		&end_revision,
		1, // svn_boolean_t discover_changed_paths, 
		1, // svn_boolean_t strict_node_history, 
		php_svn_log_message_receiver,
		(void *) return_value,
		SVN_G(ctx), SVN_G(pool));
 
	if (err) {
		svn_handle_error (err, stdout, TRUE);
		RETURN_FALSE;
	} else {
		// printf ("deployment succeeded.\n"); 
	}
	
}




/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

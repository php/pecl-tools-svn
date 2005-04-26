<?
if(!extension_loaded('svn')) {
	dl('svn.' . PHP_SHLIB_SUFFIX);
}
//svn_checkout("http://www.akbkhome.com/svn/ext_svn","/tmp/ext_svn");
//print_r(svn_cat("http://www.akbkhome.com/svn/ext_svn/svn.c"));
//print_r(svn_ls("http://www.akbkhome.com/svn/ext_svn/"));
//print_r(svn_log("http://www.akbkhome.com/svn/ext_svn"));
print_r(svn_log("http://www.akbkhome.com/svn",2));
print_r(svn_ls("http://www.akbkhome.com/svn"));
?>

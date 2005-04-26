<?
if(!extension_loaded('svn')) {
	dl('svn.' . PHP_SHLIB_SUFFIX);
}

$x = svn_repos_create('/tmp/wez-svn-foo', null, array(SVN_FS_CONFIG_FS_TYPE => SVN_FS_TYPE_FSFS));
if ($x) {
	svn_repos_recover('/tmp/wez-svn-foo');
}
exit;

//svn_checkout("http://www.akbkhome.com/svn/ext_svn","/tmp/ext_svn");
//print_r(svn_cat("http://www.akbkhome.com/svn/ext_svn/svn.c"));
//print_r(svn_ls("http://www.akbkhome.com/svn/ext_svn/"));
//print_r(svn_log("http://www.akbkhome.com/svn/ext_svn"));
print_r(svn_log("http://www.akbkhome.com/svn",2));
print_r(svn_ls("http://www.akbkhome.com/svn"));
?>

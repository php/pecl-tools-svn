<?
if(!extension_loaded('svn')) {
	dl('svn.' . PHP_SHLIB_SUFFIX);
}

if (!function_exists('file_put_contents')) {
	function file_put_contents($path, $content) {
		$fp = fopen($path, 'wb');
		fwrite($fp, $content);
		fclose($fp);
	}
}

$x = svn_repos_create('/tmp/wez-svn-foo', null, array(SVN_FS_CONFIG_FS_TYPE => SVN_FS_TYPE_FSFS));
if ($x) {
	svn_auth_set_parameter(SVN_AUTH_PARAM_DEFAULT_USERNAME, 'moriarty');
	if (svn_checkout('file:///tmp/wez-svn-foo', '/tmp/wez-svn-wd')) {
		echo "Checked out\n";
		file_put_contents('/tmp/wez-svn-wd/foobar.txt', 'hello there');
		debug_zval_dump(svn_add('/tmp/wez-svn-wd/foobar.txt'));
		print_r(svn_status('/tmp/wez-svn-wd'));
		print_r(svn_commit("testing the commit thingy\nyeeha\n", array('/tmp/wez-svn-wd/foobar.txt')));
		print_r(svn_status('/tmp/wez-svn-wd'));
		print_r(svn_log("file:///tmp/wez-svn-foo"));
	}
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

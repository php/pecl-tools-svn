--TEST--
Svn::cat()
--FILE--
<?php

function get_cat($url)
{
	ob_start();
	var_dump(Svn::cat($url));
	var_dump(Svn::cat($url, Svn::HEAD));
	var_dump(Svn::cat($url, 1));
	return ob_get_clean();
}

$repository_url = 'file://' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'r';
$filename = 'renamed_test';
$url = $repository_url . DIRECTORY_SEPARATOR . $filename;
$repository_result = get_cat($url);
echo $repository_result;

$wc_path = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc';
Svn::checkout($repository_url, $wc_path);
$url = $wc_path . DIRECTORY_SEPARATOR . $filename;
var_dump(get_cat($url) === $repository_result);

var_dump(Svn::cat($url, Svn::BASE));

/* Warning: Svn::cat(): svn error(s) occured 195012 (Two versioned resources are unrelated) Unable to find repository location for '/home/develar/pecl_svn/tests/wc/renamed_test' in revision 3 in /home/develar/pecl_svn/tests/cat.phpt on line 27 */
ini_set('error_reporting', 0);
var_dump(Svn::cat($url, Svn::PREV));

?>
--CLEAN--
<?php

include('utils.inc');
delete_directory(dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc');

?>
--EXPECT--
string(3) "who"
string(3) "who"
string(4) "rev1"
bool(true)
string(3) "who"
bool(false)
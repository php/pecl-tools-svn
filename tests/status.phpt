--TEST--
Svn::status()
--FILE--
<?php

$repository_url = 'file://' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'r';
$wc_path = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc';
Svn::checkout($repository_url, $wc_path, 2);

var_dump(Svn::status($wc_path));

$result = Svn::status($wc_path, Svn::ALL);
foreach ($result as &$item)
{
	unset($item['text_time'], $item['cmt_date']);
}
var_dump($result);

?>
--CLEAN--
<?php

include('utils.inc');
delete_directory(dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc');

?>
--EXPECT--
array(0) {
}
array(2) {
  [0]=>
  array(16) {
    ["path"]=>
    string(31) "/home/develar/pecl_svn/tests/wc"
    ["text_status"]=>
    int(3)
    ["repos_text_status"]=>
    int(1)
    ["prop_status"]=>
    int(1)
    ["repos_prop_status"]=>
    int(1)
    ["locked"]=>
    bool(false)
    ["copied"]=>
    bool(false)
    ["switched"]=>
    bool(false)
    ["name"]=>
    string(0) ""
    ["url"]=>
    string(37) "file:///home/develar/pecl_svn/tests/r"
    ["repos"]=>
    string(37) "file:///home/develar/pecl_svn/tests/r"
    ["revision"]=>
    int(2)
    ["kind"]=>
    int(2)
    ["schedule"]=>
    int(0)
    ["cmt_rev"]=>
    int(2)
    ["cmt_author"]=>
    string(7) "Develar"
  }
  [1]=>
  &array(16) {
    ["path"]=>
    string(36) "/home/develar/pecl_svn/tests/wc/test"
    ["text_status"]=>
    int(3)
    ["repos_text_status"]=>
    int(1)
    ["prop_status"]=>
    int(1)
    ["repos_prop_status"]=>
    int(1)
    ["locked"]=>
    bool(false)
    ["copied"]=>
    bool(false)
    ["switched"]=>
    bool(false)
    ["name"]=>
    string(4) "test"
    ["url"]=>
    string(42) "file:///home/develar/pecl_svn/tests/r/test"
    ["repos"]=>
    string(37) "file:///home/develar/pecl_svn/tests/r"
    ["revision"]=>
    int(2)
    ["kind"]=>
    int(1)
    ["schedule"]=>
    int(0)
    ["cmt_rev"]=>
    int(2)
    ["cmt_author"]=>
    string(7) "Develar"
  }
}
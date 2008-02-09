--TEST--
Svn::log() --revision ARG[:ARG] (depends on the Svn::checkout()) (working copy url)
--FILE--
<?php

$wc_path = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc';
Svn::checkout('file://' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'r', $wc_path);
var_dump(Svn::log($wc_path, Svn::BASE, Svn::PREV));
var_dump(Svn::log($wc_path, Svn::BASE, Svn::COMMITTED));
var_dump(Svn::log($wc_path, Svn::BASE));

?>
--CLEAN--
<?php

include('utils.inc');
delete_directory(dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc');

?>
--EXPECT--
array(2) {
  [0]=>
  array(4) {
    ["rev"]=>
    int(5)
    ["author"]=>
    string(7) "develar"
    ["msg"]=>
    string(8) "Who here"
    ["date"]=>
    string(27) "2008-01-26T08:59:34.584346Z"
  }
  [1]=>
  array(4) {
    ["rev"]=>
    int(4)
    ["author"]=>
    string(7) "Develar"
    ["msg"]=>
    string(0) ""
    ["date"]=>
    string(27) "2008-01-25T17:39:35.470636Z"
  }
}
array(1) {
  [0]=>
  array(4) {
    ["rev"]=>
    int(5)
    ["author"]=>
    string(7) "develar"
    ["msg"]=>
    string(8) "Who here"
    ["date"]=>
    string(27) "2008-01-26T08:59:34.584346Z"
  }
}
array(1) {
  [0]=>
  array(4) {
    ["rev"]=>
    int(5)
    ["author"]=>
    string(7) "develar"
    ["msg"]=>
    string(8) "Who here"
    ["date"]=>
    string(27) "2008-01-26T08:59:34.584346Z"
  }
}
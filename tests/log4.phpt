--TEST--
Svn::log() --verbose --stop-on-copy
--FILE--
<?php

$url = 'file://' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'r' . DIRECTORY_SEPARATOR . 'renamed_test';
var_dump(Svn::log($url, null, null, null, Svn::DISCOVER_CHANGED_PATHS | Svn::STOP_ON_COPY));

?>
--EXPECT--
array(1) {
  [0]=>
  array(5) {
    ["rev"]=>
    int(4)
    ["author"]=>
    string(7) "Develar"
    ["msg"]=>
    string(0) ""
    ["date"]=>
    string(27) "2008-01-25T17:39:35.470636Z"
    ["paths"]=>
    array(2) {
      [0]=>
      array(4) {
        ["action"]=>
        string(1) "A"
        ["path"]=>
        string(13) "/renamed_test"
        ["copyfrom"]=>
        string(5) "/test"
        ["rev"]=>
        int(2)
      }
      [1]=>
      array(2) {
        ["action"]=>
        string(1) "D"
        ["path"]=>
        string(5) "/test"
      }
    }
  }
}
--TEST--
Svn::log() --limit NUM
--FILE--
<?php

$url = 'file://' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'r' . DIRECTORY_SEPARATOR . 'renamed_test';
var_dump(Svn::log($url, null, null, 1));
var_dump(Svn::log($url, null, null, 3));

?>
--EXPECT--
array(1) {
  [0]=>
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
array(3) {
  [0]=>
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
  [1]=>
  array(4) {
    ["rev"]=>
    int(2)
    ["author"]=>
    string(7) "Develar"
    ["msg"]=>
    string(7) "message"
    ["date"]=>
    string(27) "2008-01-25T17:38:14.748264Z"
  }
  [2]=>
  array(4) {
    ["rev"]=>
    int(1)
    ["author"]=>
    string(7) "Develar"
    ["msg"]=>
    string(0) ""
    ["date"]=>
    string(27) "2008-01-25T17:36:40.979851Z"
  }
}
--TEST--
Svn::log() --revision ARG:ARG --quiet
--FILE--
<?php

$url = 'file://' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'r' . DIRECTORY_SEPARATOR . 'renamed_test';
var_dump(Svn::log($url, Svn::HEAD, Svn::INITIAL, null, Svn::OMIT_MESSAGES));
var_dump(Svn::log($url, Svn::HEAD, 0, null, Svn::OMIT_MESSAGES)); // use Svn::INITIAL, not magic number. it is only a test

?>
--EXPECT--
array(3) {
  [0]=>
  array(3) {
    ["rev"]=>
    int(4)
    ["author"]=>
    string(7) "Develar"
    ["date"]=>
    string(27) "2008-01-25T17:39:35.470636Z"
  }
  [1]=>
  array(3) {
    ["rev"]=>
    int(2)
    ["author"]=>
    string(7) "Develar"
    ["date"]=>
    string(27) "2008-01-25T17:38:14.748264Z"
  }
  [2]=>
  array(3) {
    ["rev"]=>
    int(1)
    ["author"]=>
    string(7) "Develar"
    ["date"]=>
    string(27) "2008-01-25T17:36:40.979851Z"
  }
}
array(3) {
  [0]=>
  array(3) {
    ["rev"]=>
    int(4)
    ["author"]=>
    string(7) "Develar"
    ["date"]=>
    string(27) "2008-01-25T17:39:35.470636Z"
  }
  [1]=>
  array(3) {
    ["rev"]=>
    int(2)
    ["author"]=>
    string(7) "Develar"
    ["date"]=>
    string(27) "2008-01-25T17:38:14.748264Z"
  }
  [2]=>
  array(3) {
    ["rev"]=>
    int(1)
    ["author"]=>
    string(7) "Develar"
    ["date"]=>
    string(27) "2008-01-25T17:36:40.979851Z"
  }
}
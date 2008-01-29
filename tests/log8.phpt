--TEST--
Svn::log() --quiet --verbose
--FILE--
<?php

$url = 'file://' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'r' . DIRECTORY_SEPARATOR . 'renamed_test';
var_dump(Svn::log($url, null, null, null, Svn::OMIT_MESSAGES | Svn::DISCOVER_CHANGED_PATHS));

?>
--EXPECT--
array(3) {
  [0]=>
  array(4) {
    ["rev"]=>
    int(4)
    ["author"]=>
    string(7) "Develar"
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
  [1]=>
  array(4) {
    ["rev"]=>
    int(2)
    ["author"]=>
    string(7) "Develar"
    ["date"]=>
    string(27) "2008-01-25T17:38:14.748264Z"
    ["paths"]=>
    array(1) {
      [0]=>
      array(2) {
        ["action"]=>
        string(1) "M"
        ["path"]=>
        string(5) "/test"
      }
    }
  }
  [2]=>
  array(4) {
    ["rev"]=>
    int(1)
    ["author"]=>
    string(7) "Develar"
    ["date"]=>
    string(27) "2008-01-25T17:36:40.979851Z"
    ["paths"]=>
    array(1) {
      [0]=>
      array(2) {
        ["action"]=>
        string(1) "A"
        ["path"]=>
        string(5) "/test"
      }
    }
  }
}
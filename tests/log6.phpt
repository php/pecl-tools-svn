--TEST--
Svn::log() --revision ARG[:ARG] (depends on the Svn::checkout())
--FILE--
<?php

$repository_url = 'file://' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'r';
$url = $repository_url . DIRECTORY_SEPARATOR . 'renamed_test';
var_dump(Svn::log($url, SvnRevision::HEAD));
var_dump(Svn::log($url, SvnRevision::HEAD, 2));
var_dump(Svn::log($url, 2, SvnRevision::HEAD));
var_dump(Svn::log($url,4, 2));
var_dump(Svn::log($url, 4));
var_dump(Svn::log($url, 4, SvnRevision::INITIAL));
var_dump(Svn::log($url, 4, 0)); // use SvnRevision::INITIAL, not magic number. it is only a test*/

$wc_path = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc';
Svn::checkout($repository_url, $wc_path);
var_dump(Svn::log($wc_path, SvnRevision::BASE, SvnRevision::PREV));
var_dump(Svn::log($wc_path, SvnRevision::BASE, SvnRevision::COMMITTED));
var_dump(Svn::log($wc_path, SvnRevision::BASE));

?>
--CLEAN--
<?php

include('utilities.php');
delete_directory(dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc');

?>
--EXPECT--
array(0) {
}
array(2) {
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
}
array(2) {
  [0]=>
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
array(2) {
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
}
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
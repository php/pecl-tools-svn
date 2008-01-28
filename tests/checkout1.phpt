--TEST--
Svn::checkout() default (depends on the Svn::status())
--FILE--
<?php

$repository_url = 'file://' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'r';
$wc_path = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc';
Svn::checkout($repository_url, $wc_path);

var_dump(file_exists($wc_path));

$status = Svn::status($wc_path, Svn::ALL);
var_dump($status[0]['revision'] === 5);

?>
--CLEAN--
<?php

include('utils.inc');
delete_directory(dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc');

?>
--EXPECT--
bool(true)
bool(true)
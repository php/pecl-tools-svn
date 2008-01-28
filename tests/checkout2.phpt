--TEST--
Svn::checkout() --revision ARG (depends on the Svn::status())
--FILE--
<?php

define('REVISION', 2);
$repository_url = 'file://' . dirname(__FILE__) . DIRECTORY_SEPARATOR . 'r';
$wc_path = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc';
Svn::checkout($repository_url, $wc_path, REVISION);

var_dump(file_exists($wc_path));

$status = Svn::status($wc_path, Svn::ALL);
var_dump($status[0]['revision'] === REVISION);

?>
--CLEAN--
<?php

include('utils.inc');
delete_directory(dirname(__FILE__) . DIRECTORY_SEPARATOR . 'wc');

?>
--EXPECT--
bool(true)
bool(true)
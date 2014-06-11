--TEST--
xcache requires deep copying static variables in shallow copy mode
--SKIPIF--
<?php
require("include-skipif.inc");
?>
--INI--
xcache.readonly_protection=0
xcache.test = 1
xcache.size = 32M
--FILE--
<?php
static $a = array(1);
echo $a[0], PHP_EOL;
?>
--EXPECT--
1

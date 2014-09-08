--TEST--
xcache shallow copying precondition: early binding changes constant inside opcode for PHP5.2-
--SKIPIF--
<?php
require("include-skipif.inc");
if (version_compare(PHP_VERSION, "5.3", ">=")) {
	die("skip only needed for PHP 5.2 or less");
}
?>
--INI--
xcache.test = 1
xcache.size = 32M
--FILE--
<?php
class A extends Exception {
}
?>
--EXPECT--

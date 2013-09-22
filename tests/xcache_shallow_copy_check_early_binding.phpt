--TEST--
xcache shallow copying precondition: early binding changes constant inside opcode for PHP5.2-
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, "5.3", ">=")) {
	die("skip only needed for PHP 5.2 or less");
}
?>
--FILE--
<?php
class A extends Exception {
}
?>
--EXPECT--

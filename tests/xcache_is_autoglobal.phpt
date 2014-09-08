--TEST--
xcache_is_autoglobal
--SKIPIF--
<?php
require("skipif.inc");
?>
--INI--
xcache.test = 1
xcache.size = 32M
--FILE--
<?php
var_dump(xcache_is_autoglobal("GLOBALS"));
?>
--EXPECT--
bool(true)

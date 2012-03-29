--TEST--
xcache_is_autoglobal
--FILE--
<?php
var_dump(xcache_is_autoglobal("GLOBALS"));
?>
--EXPECT--
bool(true)

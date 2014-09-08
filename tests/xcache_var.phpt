--TEST--
xcache_set/get test
--SKIPIF--
<?php
require("skipif.inc");
?>
--INI--
xcache.test = 1
xcache.size = 32M
xcache.var_size = 2M
--FILE--
<?php
var_dump(xcache_isset("a"));
var_dump(xcache_set("a", 1));
var_dump(xcache_get("a"));
var_dump(xcache_isset("a"));
var_dump(xcache_inc("a", 10));
var_dump(xcache_get("a"));
var_dump(xcache_dec("a", 5));
var_dump(xcache_get("a"));
xcache_unset("a");
var_dump(xcache_isset("a"));
?>
--EXPECT--
bool(false)
bool(true)
int(1)
bool(true)
int(11)
int(11)
int(6)
int(6)
bool(false)

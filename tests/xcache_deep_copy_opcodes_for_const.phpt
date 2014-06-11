--TEST--
xcache requires deep copying opcodes for __FILE__ and __DIR__
--SKIPIF--
<?php
require("include-skipif.inc");
?>
--INI--
xcache.test = 1
xcache.size = 32M
--FILE--
<?php
echo __FILE__, PHP_EOL;
?>
--EXPECTF--
%sxcache_deep_copy_opcodes_for_const%s

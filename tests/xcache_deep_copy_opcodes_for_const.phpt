--TEST--
xcache requires deep copying opcodes for __FILE__ and __DIR__
--INI--
--FILE--
<?php
echo __FILE__, PHP_EOL;
?>
--EXPECTF--
%sxcache_deep_copy_opcodes_for_const%s

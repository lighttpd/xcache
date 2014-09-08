--TEST--
xcache requires deep copying arg info
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
function a($a = 1) {
	echo $a;
}
a();
a(2);
echo PHP_EOL;
?>
--EXPECT--
12

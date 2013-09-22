--TEST--
xcache requires deep copying arg info
--INI--
xcache.readonly_protection=0
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

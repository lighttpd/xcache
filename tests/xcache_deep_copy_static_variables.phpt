--TEST--
xcache requires deep copying static variables in shallow copy mode
--INI--
xcache.readonly_protection=0
--FILE--
<?php
static $a = array(1);
echo $a[0], PHP_EOL;
?>
--EXPECT--
1

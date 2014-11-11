--TEST--
xcache_set/get test for object
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
class a
{
}

class b
{
}

$a = new a();
$b = new b();
$stdclass = new stdclass();

$b->a = $a;
$b->b = $b;
$b->array = array($b, $a);
$b->stdclass = $stdclass;

var_dump(xcache_set("a", $a));
unset($a);
var_dump($a = xcache_get("a"));

var_dump(xcache_set("b", $b));
unset($b);
var_dump($b = xcache_get("b"));

$b->test = 1;
var_dump($b->b->test);
?>
--EXPECT--
bool(true)
object(a)#4 (0) {
}
bool(true)
object(b)#7 (4) {
  ["a"]=>
  object(a)#6 (0) {
  }
  ["b"]=>
  *RECURSION*
  ["array"]=>
  array(2) {
    [0]=>
    *RECURSION*
    [1]=>
    object(a)#6 (0) {
    }
  }
  ["stdclass"]=>
  object(stdClass)#5 (0) {
  }
}
int(1)

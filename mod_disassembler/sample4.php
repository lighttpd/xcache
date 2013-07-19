<?php

class classname
{
	var $property = array(
		array('array'),
		'str'
		);

	function __construct($a, $b)
	{
		echo CONST_VALUE;
		empty($obj->objProp);
		isset($obj->objProp);
		$obj->objProp = 1;
		echo $obj->objProp;
		empty($this->thisProp);
		isset($this->thisProp);
		$this->thisProp = 1;
		echo $this->thisProp;
		unset($obj->array['index']);
		unset($this->array['index']);
		empty($_GET['get']);
		isset($_GET['get']);
		unset($_GET['get']);
		$_GET['get'] = 1;
		echo $_GET['get'];
		isset($GLOBALS['global']);
		empty($GLOBALS['global']);
		unset($GLOBALS['global']);
		$GLOBALS['global'] = 1;
		echo $GLOBALS['global'];
		empty($array['index']);
		isset($array['index']);
		unset($array['index']);
		$array['index'] = 1;
		echo $array['index'];
		empty($array['index']->indexProp);
		isset($array['index']->indexProp);
		$array['index']->indexProp = 1;
		echo $array['index']->indexProp;
		empty($GLOBALS['var']->indexProp);
		isset($GLOBALS['var']->indexProp);
		$GLOBALS['var']->indexProp = 1;
		echo $GLOBALS['var']->indexProp;
	}

	function method($a = NULL, $b = NULL)
	{
		$runtimeArray = array('1');
		$runtimeArray2 = array(
			'1',
			array()
			);
		$runtimeArray3 = array(
			'a' => '1',
			2   => array()
			);
		return 'm';
	}
}

class child extends classname
{
	function __construct()
	{
		parent::__construct();
		ClassName::__construct();
		echo __CLASS__;
		echo __METHOD__;
		echo __FUNCTION__;
		new Exception();
		$this->methodCall();
	}

	function __destruct()
	{
		parent::__destruct();
		functioncall();
	}

	function __tostring()
	{
		parent::__toString();
	}

	function __set($name, $value)
	{
	}

	function __get($name)
	{
	}

	function __isset($name)
	{
	}

	function __unset($name)
	{
	}

	function __sleep()
	{
	}

	function __wakeup()
	{
	}

	function __clone()
	{
		return array();
	}
}

function f1($f)
{
	echo __FUNCTION__;
	echo $f;
}

if ($late) {
	class LateBindingClass
	{
		function __construct()
		{
		}
	}

	function lateBindingFunction($arg)
	{
		echo 'lateFunction';
		return new lateBindingFunction();
	}
}

echo "\r\n";
echo "\r";
echo "\n";
echo str_replace(array('a' => 'a', 'b' => 'c'), 'b');
$object = new ClassName();
$object = new $className();
$a = 1;
$a = $b + $c;
$a = $b + 1;
$a = 1 + $b;
$a = $b - $c;
$a = $b * $c;
$a = $b / $c;
$a = $b % $c;
$a = $b . $c;
$a = $b = $c;
$a = $b & $c;
$a = $b | $c;
$a = $b ^ $c;
$a = ~$b;
$a = -$b;
$a = +$b;
$a = $b >> $c;
$a = $b >> $c;
$a = $b == $c;
$a = $b === $c;
$a = $b != $c;
$a = $b < $c;
$a = $b <= $c;
$a = $b <= $c;
$a = $b++;
$a = ++$b;
$a = $obj->b++;
$a = ++$obj->b;
$a = $b--;
$a = --$b;
$a = $obj->b--;
$a = --$obj->b;
$a = !$b;
$a = $b === $c;
$a = $b !== $c;
$a = $b << 2;
$a = $b >> 3;
$a += $b;
$a -= $b;
$a *= $b;
$a /= $b;
$a <<= $b;
$a >>= $b;
$a &= $b;
$a |= $b;
$a .= $b;
$a %= $b;
$a ^= $b;
$a = 'a' . 'b';
$a = 'a' . 'abc';
@f1();
print('1');
$a = $array['index'];
$a = $object->prop;
$a = $this->prop;
$array['index'] = 1;
$object->prop = 1;
$this->prop = 1;
$a = isset($b);
$a = empty($b);
unset($b);
$a = isset($array['index']);
$a = empty($array['index']);
unset($array['index']);
$a = isset($object->prop);
$a = empty($object->prop);
$a = isset($this->prop);
$a = empty($this->prop);
$a = (int) $b;
$a = (double) $b;
$a = (string) $b;
$a = (array) $b;
$a = (object) $b;
$a = (bool) $b;
$a = (unset) $b;
$a = (array) $b;
$a = (object) $b;
$a = ($b ? $c : $d);
$a = (f1() ? f2() : f3());
($a = $b) xor $c;
($a = $b) and $c;
($a = $b) or $c;
$a = $b && $c;
$a = $b || $c;

if (if_()) {
	echo 'if';

	if (innerif_()) {
		echo 'if innerIf';
	}
}
else if (elseif_()) {
	echo 'else if';

	if (innerif_()) {
		echo 'if innerIf';
	}
}
else {
	if (innerif_()) {
		echo 'if innerIf';
	}

	echo 'else';
}

while (false) {
	echo 'while';
}

do {
	echo 'do/while';
} while (false);

$i = 1;

for (; $i < 10; ++$i) {
	echo $i;
	break;
}

foreach ($array as $value) {
	foreach ($value as $key => $value) {
		echo $key . ' = ' . $value . "\n";
		break 2;
		continue;
	}
}

switch ($normalSwitch) {
case 'case1':
	echo 'case1';

	switch ($nestedSwitch) {
	case 1:
	}

	break;

case 'case2':
	echo 'case2';
	break;

default:
	switch ($nestedSwitch) {
	case 1:
	}

	echo 'default';
	break;
}

switch ($switchWithoutDefault) {
case 'case1':
	echo 'case1';
	break;

case 'case2':
	echo 'case2';
	break;
}

switch ($switchWithMiddleDefault) {
case 'case1':
	echo 'case1';
	break;

default:
	echo 'default';
	break;

case 'case2':
	echo 'case2';
	break;
}

switch ($switchWithInitialDefault) {
default:
	echo 'default';
	break;

case 'case1':
	echo 'case1';
	break;

case 'case2':
	echo 'case2';
	break;
}

switch (emptyswitch()) {
}

switch (emptyswitch()) {
default:
}

declare (ticks=1) {
	echo 1;
}

while (1) {
	declare (ticks=1) {
		echo 2;
	}
}

require 'require.php';
require_once 'require_once.php';
include 'include.php';
include_once 'include_once.php';
echo __FILE__;
echo __LINE__;
exit();

?>

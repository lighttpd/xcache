<?php
#if PHP_VERSION >= 500
#	define ClassClassName ClassName
#	define PublicMethod public
#else
#	define ClassClassName classname
#	define PublicMethod
#	define abstract
#	define innerIf_ innerif_
#	define emptySwitch emptyswitch
#	define defaultSwitch defaultswitch
#endif
#if PHP_VERSION >= 520
#else
#	define __callStatic __callstatic
#	define __toString __tostring
#endif
#if PHP_VERSION >= 530

namespace ns;
#endif

abstract class ClassClassName
{
#if PHP_VERSION >= 500
	const CONST_VALUE = 'A constant value';

	/** doc */
	static public $static = array(
		array('array'),
		'str'
		);
	/** doc */
	static public $public_static = array(2, 'str');
	/** doc */
	static private $private_static = array(2, 'str');
	/** doc */
	static protected $protected_static = array(2, 'str');
	/** doc */
	public $property = array(
		array('array'),
		'str'
		);
	/** doc */
	public $public_property = array(2, 'str');
	/** doc */
	private $private_property = array(2, 'str');
	/** doc */
	protected $protected_property = array(2, 'str');
#else
	var $property = array(
		array('array'),
		'str'
		);
#endif

#if PHP_VERSION >= 500
	/** doc */
#endif
	PublicMethod function __construct($a, $b)
	{
		echo CONST_VALUE;
#if PHP_VERSION >= 500
		echo ClassName::CONST_VALUE;
		empty(ClassName::$classProp);
		isset(ClassName::$classProp);
		unset(ClassName::$classProp);
		ClassName::$classProp = 1;
		echo ClassName::$classProp;
#endif
		empty($obj->objProp);
		isset($obj->objProp);
#if PHP_VERSION >= 500
		unset($obj->objProp);
#endif
		$obj->objProp = 1;
		echo $obj->objProp;
		empty($this->thisProp);
		isset($this->thisProp);
#if PHP_VERSION >= 500
		unset($this->thisProp);
#endif
		$this->thisProp = 1;
		echo $this->thisProp;
#if PHP_VERSION >= 500
		unset($array['index']->valueProp);
#endif
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
#if PHP_VERSION >= 500
		unset($array['index']->indexProp);
#endif
		$array['index']->indexProp = 1;
		echo $array['index']->indexProp;
		empty($GLOBALS['var']->indexProp);
		isset($GLOBALS['var']->indexProp);
#if PHP_VERSION >= 500
		unset($GLOBALS['var']->indexProp);
#endif
		$GLOBALS['var']->indexProp = 1;
		echo $GLOBALS['var']->indexProp;
		ClassName::__construct();
		echo __CLASS__;
		echo __METHOD__;
		echo __FUNCTION__;
		$this->methodCall();
#if PHP_VERSION >= 500
		throw new Exception();
		new Exception();
#endif
	}
#if PHP_VERSION >= 500

	/** doc */
	abstract public function abastractMethod();
#endif

#if PHP_VERSION >= 500
	/** doc */
#endif
	PublicMethod function method($a = NULL, $b = NULL)
	{
	}
#if PHP_VERSION >= 500

	/** doc */
	protected function protectedMethod(ClassName $a, $b = array(
			array('array')
			))
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
		return 'protected';
	}

	/** doc */
	private function privateMethod(ClassName $a, $b = NULL)
	{
		return 'private';
	}
#endif
}
#if PHP_VERSION >= 500

interface IInterface
{
	public function nothing();
}
#endif

function f1($f)
{
	echo __FUNCTION__;
	echo $f;
#if PHP_VERSION > 550

	foreach ($a as $b) {
		yield($b);
	}

	yield($f);
#endif
}
#if PHP_VERSION >= 500

final class Child extends ClassName implements IInterface
{
	public function __construct()
	{
		parent::__construct();
	}

	public function __destruct()
	{
		parent::__destruct();
		functionCall();
	}

	static public function __callStatic($name, $args)
	{
		parent::__callStatic($name, $args);
	}

	public function __toString()
	{
		parent::__toString();
	}

	public function __set($name, $value)
	{
	}

	public function __get($name)
	{
	}

	public function __isset($name)
	{
	}

	public function __unset($name)
	{
	}

	public function __sleep()
	{
	}

	public function __wakeup()
	{
	}

	public function __clone()
	{
		return array();
	}
}
#endif

if ($late) {
	class LateBindingClass
	{
		PublicMethod function __construct()
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
#if PHP_VERSION >= 500
$result = $object instanceof ClassName;
$cloned = clone $object;
#endif
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
#if PHP_VERSION >= 500
unset($object->prop);
#endif
$a = isset($this->prop);
$a = empty($this->prop);
#if PHP_VERSION >= 500
unset($this->prop);
$a = isset(ClassName::$prop);
$a = empty(ClassName::$prop);
unset(ClassName::$prop);
#endif
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
#if PHP_VERSION >= 500

do {
	try {
		echo 'outer try 1';

		try {
			echo 'inner try';
		}
		catch (InnerException $e) {
			echo $e;
		}
#if PHP_VERSION >= 550
		finally {
			echo 'inner finally';
		}
#endif

		echo 'outer try 2';
	}
	catch (OuterException $e) {
		echo $e;
	}
#if PHP_VERSION >= 550
	finally {
		echo 'outer finally';
	}
#endif
} while (0);
#endif

if (if_()) {
	echo 'if';

	if (innerIf_()) {
		echo 'if innerIf';
	}
}
else if (elseif_()) {
	echo 'else if';

	if (innerIf_()) {
		echo 'if innerIf';
	}
}
else {
	if (innerIf_()) {
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

switch (emptySwitch()) {
}

switch (defaultSwitch()) {
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
echo ____FILE____;
echo ____LINE____;
#if PHP_VERSION >= 530
echo 'PHP 5.3+ code testing';
const CONST_VALUE = 1;
echo $this::CONST_VALUE;
echo $a::CONST_VALUE;
echo CONST_VALUE;
$this::__construct();
$obj::__construct();
$a = $b ?: $d;
$a = ($b ?: $d) + $c;
$a = f1() ?: f2();
$a = C::f1() ?: C::f2();
$a = ($b ? $c : $d);
$a = ($b ? $c : $d) + $c;
$a = (f1() ? f3() : f2());

if ($b ?: $d) {
	echo 'if ($b ?: $d)';
}

if (($b ?: $d) + $c) {
	echo 'if (($b ?: $d) + $c)';
}

if (f1() ?: f2()) {
	echo 'if (f1() ?: f2())';
}

echo 'goto a';
goto a;

$i = 1;

for (; $i <= 2; ++$i) {
	goto a;
}

a:
echo 'label a';
echo preg_replace_callback('~-([a-z])~', function($match) {
	return strtoupper($match[1]);
}, 'hello-world');
$greet = function($name) {
	printf("Hello %s\r\n", $name);
};
$greet('World');
$greet('PHP');
$total = 0;
$tax = 1;
$callback = function($quantity, $product) use($tax, &$total) {
	$tax = 'tax';
	static $static1 = array(1);
	static $static2;
	$tax = 'tax';
	$tax = --$tax;
	$pricePerItem = constant('PRICE_' . strtoupper($product));
	$total += $pricePerItem * $quantity * ($tax + 1);
};
#endif
exit();

?>

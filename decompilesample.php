<?php

abstract class ClassName
{
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

	/** doc */
	public function __construct($a, $b)
	{
		echo CONST_VALUE;
		echo ClassName::CONST_VALUE;
		unset(ClassName::$classProp);
		unset($obj->objProp);
		unset($this->thisProp);
		unset($array['index']->valueProp);
		unset($obj->array['index']);
		unset($this->array['index']);
		$obj->objProp = 1;
		$this->thisProp = 1;
		$array['index']->valueProp = 1;
		$array['index'] = 1;
		$array[1] = 1;
	}

	/** doc */
	abstract public function abastractMethod();

	/** doc */
	public function method(array $a = NULL, $b = NULL)
	{
	}

	/** doc */
	public function publicMethod(ClassName $a = NULL, $b = 2)
	{
	}

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
}

interface IInterface
{
	public function nothing();
}

function f1($f)
{
	echo __FUNCTION__;
	echo $f;
}

final class Child extends ClassName implements IInterface
{
	public function __construct()
	{
		parent::__construct();
		ClassName::__construct();
		echo __CLASS__;
		echo __METHOD__;
		throw new Exception();
		$this->methodCall();
	}

	public function __destruct()
	{
		parent::__destruct();
		functionCall();
	}

	static public function __callStatic($name, $args)
	{
	}

	public function __toString()
	{
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

if ($late) {
class LateBindingClass
{}

function lateBindingFunction($arg)
{
	echo 'lateFunction';
}
}

echo "\r\n";
echo "\r";
echo "\n";
echo str_replace(array('a' => 'a', 'b' => 'c'), 'b');
$object = new ClassName();
$object = new $className();
$result = $object instanceof ClassName;
$cloned = clone $object;
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
$a = $b xor $c;
$a = !$b;
$a = $b and $c;
$a = $b or $c;
$a = $b && $c;
$a = $b || $c;
$a = $b ? $c : $d;
$a = f1() ? f2() : f3();

try {
	echo 'outer try 1';
	try {
		echo 'inner try';
	}
	catch (InnerException $e) {
		echo $e;
	}
	echo 'outer try 2';
}
catch (OuterException $e) {
	echo $e;
}

if ($a) {
	echo 'if ($a)';
}
else if ($b) {
	echo 'else if ($b)';
}
else {
	echo 'else';
}

while (false) {
	echo 'while';
}

do {
	echo 'do/while';
} while (false);

for ($i = 1; $i < 10; ++$i) {
	echo $i;
	break;
}

foreach ($array as $key => $value) {
	echo $key . ' = ' . $value . "\n";
	continue;
}

switch ($switch) {
case 'case1':
	echo 'case1';
	break;

case 'case2':
	echo 'case2';
	break;

default:
	echo 'default';
	break;
}

declare (ticks=1) {
	echo 1;
	echo 2;
}

require 'require.php';
require_once 'require_once.php';
include 'include.php';
include_once 'include_once.php';
echo __FILE__;
echo __LINE__;

//* >= PHP 5.3
echo $this::CONST_VALUE;
echo $a::CONST_VALUE;
$this::__construct();
$obj::__construct();

$a = $b ?: $d;
$a = f1() ?: f2();

echo 'goto a';
goto a;

for ($i = 1; $i <= 2; ++$i) {
	goto a;
}

a:
echo 'label a';
echo preg_replace_callback('~-([a-z])~', function ($match) {
	return strtoupper($match[1]);
}, 'hello-world');
$greet = function ($name) {
	printf("Hello %s\r\n", $name);
};
$greet('World');
$greet('PHP');
$total = 0;
$callback = function ($quantity, $product) use ($tax, &$total) {
	$pricePerItem = constant('PRICE_' . strtoupper($product));
	$total += $pricePerItem * $quantity * ($tax + 1);
};
// */

?>

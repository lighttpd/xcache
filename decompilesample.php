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
		echo $this::CONST_VALUE;
		echo $a::CONST_VALUE;
		echo ClassName::CONST_VALUE;
	}

	/** doc */
	abstract function abastractMethod();

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

final class Child extends ClassName implements IInterface
{
	public function __construct()
	{
		parent::__construct();
		echo __CLASS__;
		echo __METHOD__;
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
}

echo str_replace(array('a' => 'a', 'b' => 'c'), 'b');
$object = new ClassName();
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
$a = $b <> $c;
$a = $b < $c;
$a = $b <= $c;
$a = $b > $c;
$a = $b <= $c;
$a = $b++;
$a = ++$b;
$a = $b--;
$a = --$b;
$a = $b and $c;
$a = $b or $c;
$a = $b xor $c;
$a = !$b;
$a = $b && $c;
$a = $b || $c;
$a = $b instanceof ClassName;

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
	echo "$key = $value\n";
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

goto a;
echo 'foo';

a:
echo 'bar';

echo preg_replace_callback('~-([a-z])~', function ($match) {
		return strtoupper($match[1]);
}, 'hello-world');

$greet = function($name)
{
	printf("Hello %s\r\n", $name);
};
$greet('World');
$greet('PHP');

$total = 0.00;

$callback = function ($quantity, $product) use ($tax, &$total)
{
	$pricePerItem = constant(__CLASS__ . "::PRICE_" . strtoupper($product));
	$total += ($pricePerItem * $quantity) * ($tax + 1.0);
};

?>

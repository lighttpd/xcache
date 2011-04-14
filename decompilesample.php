<?php

class ClassName
{
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
	}

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

echo str_replace(array('a' => 'a', 'b' => 'c'), 'b');

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

?>

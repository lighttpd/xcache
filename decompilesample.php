<?php

class ClassName
{
	static public $static = array(
		array('array'),
		'str'
		);
	static public $public_static = array(2, 'str');
	static private $private_static = array(2, 'str');
	static protected $protected_static = array(2, 'str');
	public $property = array(
		array('array'),
		'str'
		);
	public $public_property = array(2, 'str');
	private $private_property = array(2, 'str');
	protected $protected_property = array(2, 'str');

	public function __construct($a, $b)
	{
	}

	function method(ClassName $a = null, $b = null)
	{
	}

	public function publicMethod(ClassName $a = null, $b = 2)
	{
	}

	protected function protectedMethod(ClassName $a, $b = array(array("array")))
	{
		return 'protected';
	}

	private function privateMethod(ClassName $a, $b = null)
	{
		return 'private';
	}
}


?>

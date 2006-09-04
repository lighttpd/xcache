<?php

function get_language_file_ex($name, $l, $s)
{
	static $map = array(
			'zh'    => 'zh-simplified',
			'zh-hk' => 'zh-traditional',
			'zh-tw' => 'zh-traditional',
			);

	if (isset($map[$l])) {
		$l = $map[$l];
	}
	if (file_exists($file = "$name-$l-$s.lang.php")) {
		return $file;
	}
	if (file_exists($file = "$name-$l.lang.php")) {
		return $file;
	}
	return null;
}

function get_language_file($name)
{
	global $charset, $lang;
	$s = strtolower($charset);
	if (isset($lang)) {
		$l = strtolower($lang);
		$file = get_language_file_ex($name, $l, $s);
		if (!isset($file)) {
			$l = strtok($l, '-');
			$file = get_language_file_ex($name, $l, $s);
		}
	}
	else if (!empty($_SERVER['HTTP_ACCEPT_LANGUAGE'])) {
		foreach (explode(',', str_replace(' ', '', $_SERVER['HTTP_ACCEPT_LANGUAGE'])) as $l) {
			$l = strtok($l, ';');
			$file = get_language_file_ex($name, $l, $s);
			if (isset($file)) {
				$lang = $l;
				break;
			}
			if (strpos($l, '-') !== false) {
				$ll = strtok($l, '-');
				$file = get_language_file_ex($name, $ll, $s);
				if (isset($file)) {
					$lang = $l;
					break;
				}
			}
		}
	}
	return isset($file) ? $file : "$name-en.lang.php";
}

function _T($str)
{
	if (isset($GLOBALS['strings'][$str])) {
		return $GLOBALS['strings'][$str];
	}
	if (!empty($GLOBALS['show_todo_strings'])) {
		return '<span style="color:red">' . htmlspecialchars($str) . '</span>';
	}
	return $str;
}

error_reporting(E_ALL);
define('REQUEST_TIME', time());

$charset = "UTF-8";
if (file_exists("./config.php")) {
	include("./config.php");
}

include(get_language_file("common"));
if (!isset($lang)) {
	$lang = 'en-us';
}

?>

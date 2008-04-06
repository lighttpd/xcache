<?php

function get_language_file_ex($name, $l, $s)
{
	static $lmap = array(
			'zh'    => 'zh-simplified',
			'zh-hk' => 'zh-traditional',
			'zh-tw' => 'zh-traditional',
			);
	static $smap = array(
			'gbk'     => 'gb2312',
			'gb18030' => 'gb2312',
			);

	if (isset($lmap[$l])) {
		$l = $lmap[$l];
	}
	if (file_exists($file = "$name-$l-$s.lang.php")) {
		return $file;
	}
	if (isset($smap[$s])) {
		$s = $smap[$s];
		if (file_exists($file = "$name-$l-$s.lang.php")) {
			return $file;
		}
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

function stripaddslashes_array($value, $mqs = false)
{
	if (is_array($value)) {
		foreach($value as $k => $v) {
			$value[$k] = stripaddslashes_array($v, $mqs);
		}
	}
	else if(is_string($value)) {
		$value = $mqs ? str_replace('\'\'', '\'', $value) : stripslashes($value);
	}
	return $value;
}

error_reporting(E_ALL);
ini_set('display_errors', 'On');
define('REQUEST_TIME', time());

if (function_exists('get_magic_quotes_gpc') && @get_magic_quotes_gpc()) {
	$mqs = (bool) ini_get('magic_quotes_sybase');
	$_GET = stripaddslashes_array($_GET, $mqs);
	$_POST = stripaddslashes_array($_POST, $mqs);
	$_REQUEST = stripaddslashes_array($_REQUEST, $mqs);
}
ini_set('magic_quotes_runtime', '0');

$charset = "UTF-8";
if (file_exists("./config.php")) {
	include("./config.php");
}

include(get_language_file("common"));
if (!isset($lang)) {
	$lang = 'en-us';
}
if (!isset($usage_graph_width) && !isset($free_graph_width)) {
	$usage_graph_width = 120;
}
$graph_width = isset($free_graph_width) ? $free_graph_width : $usage_graph_width;

?>

<?php

function xcache_validateFileName($name)
{
	return preg_match('!^[a-zA-Z0-9._-]+$!', $name);
}

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
	$file = "$name-$l-$s.lang.php";
	if (xcache_validateFileName($file) && file_exists($file)) {
		return $file;
	}
	if (isset($smap[$s])) {
		$s = $smap[$s];
		$file = "$name-$l-$s.lang.php";
		if (xcache_validateFileName($file) && file_exists($file)) {
			return $file;
		}
	}
	$file = "$name-$l.lang.php";
	if (xcache_validateFileName($file) && file_exists($file)) {
		return $file;
	}
	return null;
}

function get_language_file($name)
{
	global $config;
	$s = strtolower($config['charset']);
	if (!empty($config['lang'])) {
		$l = strtolower($config['lang']);
		$file = get_language_file_ex($name, $l, $s);
		if (!isset($file)) {
			$l = strtok($l, ':-');
			$file = get_language_file_ex($name, $l, $s);
		}
	}
	else if (!empty($_SERVER['HTTP_ACCEPT_LANGUAGE'])) {
		foreach (explode(',', str_replace(' ', '', $_SERVER['HTTP_ACCEPT_LANGUAGE'])) as $l) {
			$l = strtok($l, ':;');
			$file = get_language_file_ex($name, $l, $s);
			if (isset($file)) {
				$config['lang'] = $l;
				break;
			}
			if (strpos($l, '-') !== false) {
				$ll = strtok($l, ':-');
				$file = get_language_file_ex($name, $ll, $s);
				if (isset($file)) {
					$config['lang'] = $l;
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
	if (!empty($GLOBALS['config']['show_todo_strings'])) {
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

function ob_filter_path_nicer_default($list_html)
{
	$sep = DIRECTORY_SEPARATOR;
	$docRoot = $_SERVER['DOCUMENT_ROOT'];
	$list_html = str_replace($docRoot,  "{DOCROOT}" . (substr($docRoot, -1) == $sep ? $sep : ""), $list_html);
	$xcachedir = realpath(dirname(__FILE__) . "$sep..$sep");
	$list_html = str_replace($xcachedir . $sep, "{XCache}$sep", $list_html);
	if ($sep == '/') {
		$list_html = str_replace("/home/", "{H}/", $list_html);
	}
	return $list_html;
}


error_reporting(E_ALL);
ini_set('display_errors', 'On');
define('REQUEST_TIME', time());

if (function_exists('get_magic_quotes_gpc') && @get_magic_quotes_gpc()) {
	$mqs = (bool) ini_get('magic_quotes_sybase');
	$_GET = stripaddslashes_array($_GET, $mqs);
	$_POST = stripaddslashes_array($_POST, $mqs);
	$_REQUEST = stripaddslashes_array($_REQUEST, $mqs);
	unset($mqs);
}
ini_set('magic_quotes_runtime', '0');

$config = array();
include("./config.default.php");
if (file_exists("./config.php")) {
	include("./config.php");
}

include(get_language_file("common"));
if (empty($config['lang'])) {
	$config['lang'] = 'en-us';
}

?>

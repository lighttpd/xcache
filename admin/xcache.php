<?php

include("./common.php");

class Cycle
{
	var $values;
	var $i;
	var $count;

	function Cycle($v)
	{
		$this->values = func_get_args();
		$this->i = -1;
		$this->count = count($this->values);
	}

	function next()
	{
		$this->i = ($this->i + 1) % $this->count;
		return $this->values[$this->i];
	}

	function cur()
	{
		return $this->values[$this->i];
	}

	function reset()
	{
		$this->i = -1;
	}
}

function number_formats($a, $keys)
{
	foreach ($keys as $k) {
		$a[$k] = number_format($a[$k]);
	}
	return $a;
}

function size($size)
{
	$size = (int) $size;
	if ($size < 1024)
		return number_format($size, 2) . ' b';

	if ($size < 1048576)
		return number_format($size / 1024, 2) . ' K';

	return number_format($size / 1048576, 2) . ' M';
}

function age($time)
{
	if (!$time) return '';
	$delta = REQUEST_TIME - $time;

	if ($delta < 0) {
		$delta = -$delta;
	}
	
  	static $seconds = array(1, 60, 3600, 86400, 604800, 2678400, 31536000);
	static $name = array('s', 'm', 'h', 'd', 'w', 'M', 'Y');

	for ($i = 6; $i >= 0; $i --) {
		if ($delta >= $seconds[$i]) {
			$ret = (int) ($delta / $seconds[$i]);
			return $ret . ' ' . $name[$i];
		}
	}

	return '0 s';
}

function switcher($name, $options)
{
	$n = isset($_GET[$name]) ? $_GET[$name] : null;
	$html = array();
	foreach ($options as $k => $v) {
		$html[] = sprintf('<a href="?%s=%s"%s>%s</a>', $name, $k, $k == $n ? ' class="active"' : '', $v);
	}
	return implode(' ', $html);
}

if (!extension_loaded('XCache')) {
	echo '<h1>XCache is not loaded</h1>';
	ob_start();
	phpinfo();
	$info = ob_get_clean();
	if (preg_match('!<td class="v">(.*\\.ini)!', $info, $m)) {
		echo "Please check $m[1]";
	}
	else {
		echo "You don't even have a php.ini yet?";
	}
	exit;
}
$pcnt = xcache_count(XC_TYPE_PHP);
$vcnt = xcache_count(XC_TYPE_VAR);

$moduleinfo = null;
$type_none = -1;
if (!isset($_GET['type'])) {
	$_GET['type'] = $type_none;
}
$_GET['type'] = $type = (int) $_GET['type'];

// {{{ process clear
function processClear()
{
	$type = isset($_POST['type']) ? $_POST['type'] : null;
	if ($type != XC_TYPE_PHP && $type != XC_TYPE_VAR) {
		$type = null;
	}
	if (isset($type)) {
		$cacheid = (int) (isset($_POST['cacheid']) ? $_POST['cacheid'] : 0);
		if (isset($_POST['clearcache'])) {
			xcache_clear_cache($type, $cacheid);
		}
	}
}
processClear();
// }}}
// {{{ load info/list
$cacheinfos = array();
for ($i = 0; $i < $pcnt; $i ++) {
	$data = xcache_info(XC_TYPE_PHP, $i);
	if ($type === XC_TYPE_PHP) {
		$data += xcache_list(XC_TYPE_PHP, $i);
	}
	$data['type'] = XC_TYPE_PHP;
	$data['cache_name'] = "php#$i";
	$data['cacheid'] = $i;
	$cacheinfos[] = $data;
}
for ($i = 0; $i < $vcnt; $i ++) {
	$data = xcache_info(XC_TYPE_VAR, $i);
	if ($type === XC_TYPE_VAR) {
		$data += xcache_list(XC_TYPE_VAR, $i);
	}
	$data['type'] = XC_TYPE_VAR;
	$data['cache_name'] = "var#$i";
	$data['cacheid'] = $i;
	$cacheinfos[] = $data;
}
// }}}
// {{{ merge the list
switch ($type) {
case XC_TYPE_PHP:
case XC_TYPE_VAR:
	$cachelist = array('type' => $type, 'cache_list' => array(), 'deleted_list' => array());
	if ($type == XC_TYPE_VAR) {
		$cachelist['type_name'] = 'var';
	}
	else {
		$cachelist['type_name'] = 'php';
	}
	foreach ($cacheinfos as $i => $c) {
		if ($c['type'] == $type && isset($c['cache_list'])) {
			foreach ($c['cache_list'] as $e) {
				$e['cache_name'] = $c['cache_name'];
				$cachelist['cache_list'][] = $e;
			}
			foreach ($c['deleted_list'] as $e) {
				$e['cache_name'] = $c['cache_name'];
				$cachelist['deleted_list'][] = $e;
			}
		}
	}
	if ($type == XC_TYPE_PHP) {
		$inodes = array();
		$haveinode = false;
		foreach ($cachelist['cache_list'] as $e) {
			if (isset($e['inode'])) {
				$haveinode = true;
			}
		}
		if (!$haveinode) {
			foreach ($cachelist['deleted_list'] as $e) {
				if (isset($e['inode'])) {
					$haveinode = true;
				}
			}
		}
	}
	unset($data);
	break;

default:
	$_GET['type'] = $type_none;
	$cachelist = array();
	ob_start();
	phpinfo(INFO_MODULES);
	$moduleinfo = ob_get_clean();
	if (preg_match('!XCache</a></h2>(.*?)<h2><a name="module_!is', $moduleinfo, $m)) {
		$moduleinfo = $m[1];
	}
	else {
		$moduleinfo = null;
	}
	break;
}
// }}}

$type_php = XC_TYPE_PHP;
$type_var = XC_TYPE_VAR;
$types = array($type_none => _T('Statistics'), $type_php => _T('List PHP'), $type_var => _T('List Var Data'));
$xcache_version = XCACHE_VERSION;
$xcache_modules = XCACHE_MODULES;

include("xcache.tpl.php");

?>

<?php

include "./common.php";

function freeblock_to_graph($freeblocks, $size) // {{{
{
	global $config;

	// cached in static variable
	static $graph_initial;
	if (!isset($graph_initial)) {
		$graph_initial = array_fill(0, $config['percent_graph_width'], 0);
	}
	$graph = $graph_initial;
	foreach ($freeblocks as $b) {
		$begin = $b['offset'] / $size * $config['percent_graph_width'];
		$end = ($b['offset'] + $b['size']) / $size * $config['percent_graph_width'];

		if ((int) $begin == (int) $end) {
			$v = $end - $begin;
			$graph[(int) $v] += $v - (int) $v;
		}
		else {
			$graph[(int) $begin] += 1 - ($begin - (int) $begin);
			$graph[(int) $end] += $end - (int) $end;
			for ($i = (int) $begin + 1, $e = (int) $end; $i < $e; $i ++) {
				$graph[$i] += 1;
			}
		}
	}
	$html = array();
	$c = 255;
	foreach ($graph as $k => $v) {
		if ($config['percent_graph_type'] != 'free') {
			$v = 1 - $v;
		}
		$v = (int) ($v * $c);
		$r = $g = $c - $v;
		$b = $c;
		$html[] = '<div style="background: rgb(' . "$r,$g,$b" . ')"></div>';
	}
	return implode('', $html);
}
// }}}
function calc_total(&$total, $data) // {{{
{
	foreach ($data as $k => $v) {
		switch ($k) {
		case 'type':
		case 'cache_name':
		case 'cacheid':
		case 'free_blocks':
			continue 2;
		}
		if (!isset($total[$k])) {
			$total[$k] = $v;
		}
		else {
			switch ($k) {
			case 'hits_by_hour':
			case 'hits_by_second':
				foreach ($data[$k] as $kk => $vv) {
					$total[$k][$kk] += $vv;
				}
				break;

			default:
				$total[$k] += $v;
			}
		}
	}
}
// }}}
function array_avg($a) // {{{
{
	if (count($a) == 0) {
		return '';
	}
	return array_sum($a) / count($a);
}
// }}}
function bar_hits_percent($v, $percent, $active) // {{{
{
	$r = 220 + (int) ($percent * 25);
	$g = $b = 220 - (int) ($percent * 220);
	$percent = (int) ($percent * 100);
	$a = $active ? ' class="active"' : '';
	$height = 20 - 1 * 2;
	$valueHeight = ceil($height * $percent / 100);
	$paddingHeight = $height - $valueHeight;
	$valueHeight = $valueHeight ? $valueHeight . "px" : 0;
	$paddingHeight = $paddingHeight ? $paddingHeight . "px" : 0;
	return '<a title="' . $v . '" href="javascript:;"' . $a . '>'
		. ($paddingHeight ? '<div style="height: ' . $paddingHeight . '"></div>' : '')
		. ($valueHeight ? '<div style="background: rgb(' . "$r,$g,$b" . '); height: ' . $valueHeight . '"></div>' : '')
		. '</a>';
}
// }}}
function get_cache_hits_graph($ci, $key) // {{{
{
	global $maxHitsByHour;
	if ($ci['cacheid'] == -1) {
		$max = max($ci[$key]);
	}
	else {
		$max = $maxHitsByHour[$ci['type']];
	}
	if (!$max) {
		$max = 1;
	}
	$t = (time() / (60 * 60)) % 24;

	$html = array();
	static $cssOut = false;
	if (!$cssOut) {
		$cssOut = true;
		$html[] = '<style type="text/css">';
		$html[] = '.hitsgraph a { width: 2px; height: 20px; border-top-width: 1px; border-bottom-width: 1px; }';
		$html[] = '</style>';
	}

	$width = count($ci[$key]) * 2;
	$html[] = '<div class="hitsgraph" style="width: ' . $width . 'px">';
	foreach ($ci[$key] as $i => $v) {
		$html[] = bar_hits_percent($v, $v / $max, $i == $t);
	}
	$html[] = "</div>";
	return implode('', $html);
}
// }}}
function getModuleInfo() // {{{
{
	ob_start();
	phpinfo(INFO_MODULES);
	$moduleInfo = ob_get_clean();
	if (!preg_match_all('!(XCache[^<>]*)</a></h2>(.*?)<h2>!is', $moduleInfo, $m)) {
		return;
	}

	$moduleInfo = array();
	foreach ($m[1] as $i => $dummy) {
		$caption = trim($m[1][$i]);
		$info = str_replace('<br />', '', trim($m[2][$i]));

		$regex = '!<table[^>]*>!';
		if (preg_match($regex, $info)) {
			$moduleInfo[] = preg_replace($regex, "\\0<caption>$caption</caption>", $info, 1);
		}
		else {
			$moduleInfo[] = "<h3>$caption</h3>";
			$moduleInfo[] = $info;
		}
	}
	return implode('', $moduleInfo);
}
// }}}
function getCacheInfos() // {{{
{
	static $cacheInfos;
	if (isset($cacheInfos)) {
		return $cacheInfos;
	}

	$phpCacheCount = xcache_count(XC_TYPE_PHP);
	$varCacheCount = xcache_count(XC_TYPE_VAR);

	$cacheInfos = array();
	$total = array();
	global $maxHitsByHour;
	$maxHitsByHour = array(0, 0);
	for ($i = 0; $i < $phpCacheCount; $i ++) {
		$data = xcache_info(XC_TYPE_PHP, $i);
		if ($_GET['do'] === 'listphp') {
			$data += xcache_list(XC_TYPE_PHP, $i);
		}
		$data['type'] = XC_TYPE_PHP;
		$data['cache_name'] = "php#$i";
		$data['cacheid'] = $i;
		$cacheInfos[] = $data;
		$maxHitsByHour[XC_TYPE_PHP] = max($maxHitsByHour[XC_TYPE_PHP], max($data['hits_by_hour']));
		if ($phpCacheCount >= 2) {
			calc_total($total, $data);
		}
	}

	if ($phpCacheCount >= 2) {
		$total['type'] = XC_TYPE_PHP;
		$total['cache_name'] = _('Total');
		$total['cacheid'] = -1;
		$total['gc'] = null;
		$total['istotal'] = true;
		unset($total['compiling']);
		$cacheInfos[] = $total;
	}

	$total = array();
	for ($i = 0; $i < $varCacheCount; $i ++) {
		$data = xcache_info(XC_TYPE_VAR, $i);
		if ($_GET['do'] === 'listvar') {
			$data += xcache_list(XC_TYPE_VAR, $i);
		}
		$data['type'] = XC_TYPE_VAR;
		$data['cache_name'] = "var#$i";
		$data['cacheid'] = $i;
		$cacheInfos[] = $data;
		$maxHitsByHour[XC_TYPE_VAR] = max($maxHitsByHour[XC_TYPE_VAR], max($data['hits_by_hour']));
		if ($varCacheCount >= 2) {
			calc_total($total, $data);
		}
	}

	if ($varCacheCount >= 2) {
		$total['type'] = XC_TYPE_VAR;
		$total['cache_name'] = _('Total');
		$total['cacheid'] = -1;
		$total['gc'] = null;
		$total['istotal'] = true;
		$cacheInfos[] = $total;
	}
	return $cacheInfos;
}
// }}}
function getEntryList() // {{{
{
	static $entryList;
	if (isset($entryList)) {
		return $entryList;
	}
	$entryList = array('cache_list' => array(), 'deleted_list' => array());
	if ($_GET['do'] == 'listphp') {
		$entryList['type_name'] = 'php';
		$entryList['type'] = XC_TYPE_PHP;
	}
	else {
		$entryList['type_name'] = 'var';
		$entryList['type'] = XC_TYPE_VAR;
	}
	foreach (getCacheInfos() as $i => $c) {
		if (!empty($c['istotal'])) {
			continue;
		}
		if ($c['type'] == $entryList['type'] && isset($c['cache_list'])) {
			foreach ($c['cache_list'] as $e) {
				$e['cache_name'] = $c['cache_name'];
				$entryList['cache_list'][] = $e;
			}
			foreach ($c['deleted_list'] as $e) {
				$e['cache_name'] = $c['cache_name'];
				$entryList['deleted_list'][] = $e;
			}
		}
	}
	return $entryList;
}
// }}}

$module = "cacher";
xcache_count(XC_TYPE_PHP); // trigger auth
if (!extension_loaded('XCache')) {
	include "../common/header.tpl.php";
	echo '<h1>XCache is not loaded</h1>';
	ob_start();
	phpinfo(INFO_GENERAL);
	$info = ob_get_clean();
	if (preg_match_all("!<tr>[^<]*<td[^>]*>[^<]*(?:Configuration|ini|Server API)[^<]*</td>[^<]*<td[^>]*>[^<]*</td>[^<]*</tr>!s", $info, $m)) {
		echo '<div class="phpinfo">';
		echo 'PHP Info';
		echo '<table>';
		echo implode('', $m[0]);
		echo '</table>';
		echo '</div>';
	}
	if (preg_match('!<td class="v">(.*?\\.ini)!', $info, $m)) {
		echo "Please check $m[1]";
	}
	else if (preg_match('!Configuration File \\(php.ini\\) Path *</td><td class="v">([^<]+)!', $info, $m)) {
		echo "Please put a php.ini in $m[1] and load XCache extension";
	}
	else {
		echo "You don't even have a php.ini yet?";
	}
	echo "(See above)";
	include "../common/footer.tpl.php";
	exit;
}

$doTypes = array(
		'' => _('Summary'),
		'listphp' => _('List PHP'),
		'listvar' => _('List Var Data'),
		'help' => _('Help'),
		);

function processPOST() // {{{
{
	if (isset($_POST['remove']) && is_array($_POST['remove'])) {
		foreach ($_POST['remove'] as $name) {
			if (is_string($name)) {
				xcache_unset($name);
			}
		}
	}

	$type = isset($_POST['type']) ? $_POST['type'] : null;
	if ($type != XC_TYPE_PHP && $type != XC_TYPE_VAR) {
		$type = null;
	}
	if (isset($type)) {
		$cacheid = (int) (isset($_POST['cacheid']) ? $_POST['cacheid'] : 0);
		if (isset($_POST['clearcache'])) {
			xcache_clear_cache($type, $cacheid);
		}
		if (isset($_POST['enable'])) {
			xcache_enable_cache($type, $cacheid);
		}
		if (isset($_POST['disable'])) {
			xcache_enable_cache($type, $cacheid, false);
		}
	}

	if (isset($_POST['coredump'])) {
		xcache_coredump();
	}
}
// }}}

processPOST();

if (!isset($_GET['do'])) {
	$_GET['do'] = '';
}

switch ($_GET['do']) {
case 'listphp':
case 'listvar':
	include "./listentries.tpl.php";
	break;

case 'help':
	include "./help.tpl.php";
	break;

default:
	include "./summary.tpl.php";
	break;
}

?>

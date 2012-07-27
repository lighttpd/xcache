<?php

include("./common.php");

function freeblock_to_graph($freeblocks, $size)
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

function calc_total(&$total, $data)
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

function array_avg($a)
{
	if (count($a) == 0) {
		return '';
	}
	return array_sum($a) / count($a);
}

function bar_hits_percent($v, $percent, $active)
{
	$r = 220 + (int) ($percent * 25);
	$g = $b = 220 - (int) ($percent * 220);
	$percent = (int) ($percent * 100);
	$a = $active ? ' active' : '';
	return '<div title="' . $v . '">'
		. '<div class="barf' . $a . '" style="height: ' . (100 - $percent) . '%"></div>'
		. '<div class="barv' . $a . '" style="background: rgb(' . "$r,$g,$b" . '); height: ' . $percent . '%"></div>'
		. '</div>';
}

function get_cache_hits_graph($ci, $key)
{
	if ($ci['cacheid'] == -1) {
		$max = max($ci[$key]);
	}
	else {
		$max = $GLOBALS['maxhits_by_hour'][$ci['type']];
	}
	if (!$max) {
		$max = 1;
	}
	$t = (time() / (60 * 60)) % 24;
	$html = array();
	foreach ($ci[$key] as $i => $v) {
		$html[] = bar_hits_percent($v, $v / $max, $i == $t);
	}
	return implode('', $html);
}

$module = "cacher";
if (!extension_loaded('XCache')) {
	include("../common/header.tpl.php");
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
	include("../common/footer.tpl.php");
	exit;
}
$pcnt = xcache_count(XC_TYPE_PHP);
$vcnt = xcache_count(XC_TYPE_VAR);

if ($_SERVER['REQUEST_METHOD'] == 'POST') {
	$remove = @ $_POST['remove'];
	if ($remove && is_array($remove)) {
		foreach ($remove as $name) {
			xcache_unset($name);
		}
	}
}

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
			$count = xcache_count($type);
			if ($cacheid >= 0) {
				for ($cacheid = 0; $cacheid < $count; $cacheid ++) {
					xcache_clear_cache($type, $cacheid);
				}
			}
			else {
				xcache_clear_cache($type);
			}
		}
	}
}
processClear();
// }}}
// {{{ load info/list
$cacheinfos = array();
$total = array();
$maxhits_by_hour = array(0, 0);
for ($i = 0; $i < $pcnt; $i ++) {
	$data = xcache_info(XC_TYPE_PHP, $i);
	if ($type === XC_TYPE_PHP) {
		$data += xcache_list(XC_TYPE_PHP, $i);
	}
	$data['type'] = XC_TYPE_PHP;
	$data['cache_name'] = "php#$i";
	$data['cacheid'] = $i;
	$cacheinfos[] = $data;
	$maxhits_by_hour[XC_TYPE_PHP] = max($maxhits_by_hour[XC_TYPE_PHP], max($data['hits_by_hour']));
	if ($pcnt >= 2) {
		calc_total($total, $data);
	}
}

if ($pcnt >= 2) {
	$total['type'] = XC_TYPE_PHP;
	$total['cache_name'] = _('Total');
	$total['cacheid'] = -1;
	$total['gc'] = null;
	$total['istotal'] = true;
	$cacheinfos[] = $total;
}

$total = array();
for ($i = 0; $i < $vcnt; $i ++) {
	$data = xcache_info(XC_TYPE_VAR, $i);
	if ($type === XC_TYPE_VAR) {
		$data += xcache_list(XC_TYPE_VAR, $i);
	}
	$data['type'] = XC_TYPE_VAR;
	$data['cache_name'] = "var#$i";
	$data['cacheid'] = $i;
	$cacheinfos[] = $data;
	$maxhits_by_hour[XC_TYPE_VAR] = max($maxhits_by_hour[XC_TYPE_VAR], max($data['hits_by_hour']));
	if ($vcnt >= 2) {
		calc_total($total, $data);
	}
}

if ($vcnt >= 2) {
	$total['type'] = XC_TYPE_VAR;
	$total['cache_name'] = _('Total');
	$total['cacheid'] = -1;
	$total['gc'] = null;
	$total['istotal'] = true;
	$cacheinfos[] = $total;
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
		if (!empty($c['istotal'])) {
			continue;
		}
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
			if (isset($e['file_inode'])) {
				$haveinode = true;
				break;
			}
		}
		if (!$haveinode) {
			foreach ($cachelist['deleted_list'] as $e) {
				if (isset($e['file_inode'])) {
					$haveinode = true;
					break;
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
	if (preg_match_all('!(XCache[^<>]*)</a></h2>(.*?)<h2>!is', $moduleinfo, $m)) {
		$moduleinfo = array();
		foreach ($m[1] as $i => $dummy) {
			$moduleinfo[] = '<h3>' . trim($m[1][$i]) . '</h3>';
			$moduleinfo[] = str_replace('<br />', '', trim($m[2][$i]));
		}
		$moduleinfo = implode('', $moduleinfo);
	}
	else {
		$moduleinfo = null;
	}
	break;
}
// }}}

$type_php = XC_TYPE_PHP;
$type_var = XC_TYPE_VAR;
$listTypes = array($type_none => _('Statistics'), $type_php => _('List PHP'), $type_var => _('List Var Data'));

include("cacher.tpl.php");

?>

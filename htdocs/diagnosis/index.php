<?php

include "../common/common.php";

$module = "diagnosis";

$notes = array();
function note($type, $reason, $suggestion = "ok") // {{{
{
	global $notes;
	$notes[] = array(
			'type' => $type
			, 'reason' => $reason
			, 'suggestion' => $suggestion
			);
}
// }}}
function getCacheInfos() // {{{
{
	$phpCacheCount = xcache_count(XC_TYPE_PHP);
	$varCacheCount = xcache_count(XC_TYPE_VAR);

	$cacheInfos = array();
	for ($i = 0; $i < $phpCacheCount; $i ++) {
		$cacheInfo = xcache_info(XC_TYPE_PHP, $i);
		$cacheInfo['type'] = XC_TYPE_PHP;
		$cacheInfos[] = $cacheInfo;
	}

	for ($i = 0; $i < $varCacheCount; $i ++) {
		$cacheInfo = xcache_info(XC_TYPE_VAR, $i);
		$cacheInfo['type'] = XC_TYPE_VAR;
		$cacheInfos[] = $cacheInfo;
	}
	return $cacheInfos;
}
// }}}

if (!extension_loaded('XCache')) {
	ob_start();
	phpinfo(INFO_GENERAL);
	$info = ob_get_clean();
	ob_start();
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
	note("error", _T('XCache is not loaded'), ob_get_clean());
}
else {
	note("info", _T('XCache loaded'));

	$cacheInfos = getCacheInfos();

	if (!ini_get("xcache.size") || !ini_get("xcache.cacher")) {
		note(
			"error"
			, _T("XCache is not enabled. Website is not accelerated by XCache")
			, _T("Set xcache.size to non-zero, set xcache.cacher = On")
			);
	}
	else {
		note("info", _T('XCache Enabled'));
	}

	$ooms = false;
	foreach ($cacheInfos as $cacheInfo) {
		if ($cacheInfo['ooms']) {
			$ooms = true;
			break;
		}
	}
	if ($ooms) {
		note(
			"warning"
			, _T("Out of memory happened when trying to write to cache")
			, "Increase xcache.size and/or xcache.var_size"
			);
	}
	else {
		note("info", _T('XCache Memory Size'));
	}

	$errors = false;
	foreach ($cacheInfos as $cacheInfo) {
		if ($cacheInfo['errors']) {
			$errors = true;
			break;
		}
	}
	if ($errors) {
		note(
			"warning"
			, _T("Error happened when compiling at least one of your PHP code")
			, _T("This usually means there is syntax error in your PHP code. Enable PHP error_log to see what parser error is it, fix your code")
			);
	}
	else {
		note("info", _T('All PHP scripts seem fine'));
	}

	/*
	if ($ini['xcache.count'] < cpucount() * 2) {
	}

	if ($ini['xcache.size'] is small $ini['xcache.slots'] is big) {
	}

	if ($ini['xcache.readonly_protection']) {
	}

	if ($cache['compiling']) {
	}

	if ($cache['compiling']) {
	}

	if ($cache['disabled']) {
	}
	*/
}

/*

if (($coredumpFiles = globCoreDumpFiles()) {
}

if (module not for server) {
}

$phpVersion = php_version();
foreach ($knownUnstablePhpVersion as $unstablePhpVersion => $reason) {
	if (substr($phpVersion, 0, strlen($unstablePhpVersion)) == $unstablePhpVersion) {
		// ..
	}
}

if (Zend Optimizer is loaded, zend optimize level > 0) {
	warn("disabled");
}

*/

include "./diagnosis.tpl.php";


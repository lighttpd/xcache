<?php

include "../common/common.php";

$module = "diagnosis";

$notes = array();
function note($type, $reason, $suggestion)
{
	global $notes;
	$notes[] = array(
			'type' => $type
			, 'reason' => $reason
			, 'suggestion' => $suggestion
			);
}

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

$cacheInfos = getCacheInfos();

// if (!$ini['xcache.size'] || !$ini['xcache.cacher']) {

foreach ($cacheInfos as $cacheInfo) {
	if ($cacheInfo['ooms']) {
		note(
			"warning"
			, "Out of memory happened when trying to write to cache"
			, "Increase xcache.size and/or xcache.var_size"
			);
		break;
	}
}

foreach ($cacheInfos as $cacheInfo) {
	if ($cacheInfo['errors']) {
		note(
			"warning"
			, "Error happened when compiling your PHP code"
			, "This usually means there is syntax error in your PHP code. Enable PHP error_log to see what parser error is it, fix your code"
			);
		break;
	}
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


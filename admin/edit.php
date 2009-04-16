<?php

include("./common.php");

if (!isset($_GET['name'])) {
	die("missing name");
}

$name = $_GET['name'];
// trigger auth
$vcnt = xcache_count(XC_TYPE_VAR);

if ($_SERVER['REQUEST_METHOD'] == 'POST') {
	if ($enable_eval) {
		eval('$value = ' . $_POST['value']);
	}
	else {
		$value = $_POST['value'];
	}
	xcache_set($name, $value);
	header("Location: xcache.php?type=" . XC_TYPE_VAR);
	exit;
}
$value = xcache_get($name);
if ($enable_eval) {
	$value = var_export($value, true);
	$editable = true;
}
else {
	$editable = is_string($value);
}

$xcache_version = XCACHE_VERSION;
$xcache_modules = XCACHE_MODULES;

include("edit.tpl.php");

?>

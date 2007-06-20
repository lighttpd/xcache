<?php

include("./common.php");

if (!isset($_GET['name'])) {
	die("missing name");
}

$name = $_GET['name'];
// trigger auth
$vcnt = xcache_count(XC_TYPE_VAR);

if ($_SERVER['REQUEST_METHOD'] == 'POST') {
	eval('$value = ' . $_POST['value']);
	xcache_set($name, $value);
	header("Location: xcache.php?type=" . XC_TYPE_VAR);
	exit;
}
$value = var_export(xcache_get($name), true);

$xcache_version = XCACHE_VERSION;
$xcache_modules = XCACHE_MODULES;

include("edit.tpl.php");

?>

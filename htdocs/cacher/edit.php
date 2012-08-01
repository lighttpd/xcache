<?php

include "./common.php";

if (!isset($_GET['name'])) {
	die("missing name");
}

$name = $_GET['name'];
// trigger auth
$vcnt = xcache_count(XC_TYPE_VAR);

if ($_SERVER['REQUEST_METHOD'] == 'POST') {
	if (!empty($config['enable_eval'])) {
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
if (!empty($enable['enable_eval'])) {
	$value = var_export($value, true);
	$editable = true;
}
else {
	if (is_string($value)) {
		$editable = true;
	}
	else {
		$editable = false;
		$value = var_export($value, true);
	}
}

include "edit.tpl.php";

?>

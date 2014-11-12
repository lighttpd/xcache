<?php
if (version_compare(phpversion(), '5.2', '>=') && !extension_loaded("xcache")) {
	die("skip xcache extension not loaded");
}
?>

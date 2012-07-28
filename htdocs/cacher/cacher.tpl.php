<?php include("../common/header.tpl.php"); ?>
<div class="switcher"><?php echo switcher("type", $listTypes); ?></div>
<div id="help">
	<a href="help.php"><?php echo _("Help") ?> &raquo;</a>
</div>
<?php
include "./cacher.summary.tpl.php";
if ($cachelist) {
	$isphp = $cachelist['type'] == $type_php;
	ob_start($config['path_nicer']);

	$listName = 'Cached';
	$entries = $cachelist['cache_list'];
	include "./cacher.entrylist.tpl.php";

	$listName = 'Deleted';
	$entries = $cachelist['deleted_list'];
	include "./cacher.entrylist.tpl.php";

	ob_end_flush();
	unset($isphp);
}

if ($moduleinfo) {
	if (ini_get("xcache.test")) {
?>
<form method="post" action="">
	<div>
		<input type="submit" name="coredump" value="Test coredump" class="submit" onclick="return confirm('<?php echo _('Sure?'); ?>');" />
	</div>
</form>
<?php
	}
	$t_moduleinfo = _("Module Info");
	echo <<<HTML
<h2>$t_moduleinfo</h2>
<div class="phpinfo">$moduleinfo</div>
HTML;
}
?>
<?php include("../common/footer.tpl.php"); ?>

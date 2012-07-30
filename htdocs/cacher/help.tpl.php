<?php include "../common/header.tpl.php"; ?>
<div class="switcher"><?php echo switcher("do", $doTypes); ?></div>
<?php if (ini_get("xcache.test")) { ?>
<form method="post" action="">
	<div>
		<input type="submit" name="coredump" value="Test coredump" class="submit" onclick="return confirm('<?php echo _T('Sure?'); ?>');" />
	</div>
</form>
<?php } ?>
<div id="help">
	<?php include get_language_file("help"); ?>
</div>
<?php include "../common/footer.tpl.php" ?>

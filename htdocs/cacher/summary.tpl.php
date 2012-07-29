<?php include("../common/header.tpl.php"); ?>
<div class="switcher"><?php echo switcher("do", $listTypes); ?></div>
<?php
include "./sub/summary.tpl.php";

if (ini_get("xcache.test")) {
?>
<form method="post" action="">
	<div>
		<input type="submit" name="coredump" value="Test coredump" class="submit" onclick="return confirm('<?php echo _('Sure?'); ?>');" />
	</div>
</form>
<?php
}
?>
<?php include("./sub/moduleinfo.tpl.php"); ?>

<?php include("../common/footer.tpl.php"); ?>

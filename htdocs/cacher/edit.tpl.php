<?php include("header.tpl.php"); ?>
<?php
$h_name = htmlspecialchars($name);
$h_value = htmlspecialchars($value);
?>
<form method="post" action="">
	<fieldset>
		<legend><?php echo sprintf(_("Editing Variable %s"), $h_name); ?></legend>
		<textarea name="value" style="width: 100%; height: 200px; overflow-y: auto" <?php echo $editable ? "" : "disabled=disabled"; ?>><?php echo $h_value; ?></textarea><br>
		<input type="submit" <?php echo $editable ? "" : "disabled=disabled"; ?>>
		<?php
		if (!$editable) {
			echo sprintf(_("Set %s in config to enable"), "\$config['enable_eval'] = true");
		}
		?>
	</fieldset>
</form>
<?php include("footer.tpl.php"); ?>

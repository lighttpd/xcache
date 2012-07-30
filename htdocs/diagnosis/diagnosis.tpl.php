<?php include "../common/header.tpl.php"; ?>
<table cellspacing="0" cellpadding="4" class="cycles">
	<caption>
		<?php echo _T("Diagnosis Result"); ?>
	</caption>
	<tr>
		<th>
			<?php echo _T("Level"); ?>
		</th>
		<th>
			<?php echo _T("Reason"); ?>
		</th>
		<th>
			<?php echo _T("Suggestion"); ?>
		</th>
	</tr>
<?php foreach ($notes as $note) { ?>
	<tr>
		<td><?php echo ucfirst($note['type']); ?></td>
		<td><?php echo $note['reason']; ?></td>
		<td><?php echo $note['suggestion']; ?></td>
	</tr>
<?php } ?>
</table>
<?php include "../common/footer.tpl.php"; ?>

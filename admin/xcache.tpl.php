<!DOCTYPE HTML PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
	<meta http-equiv="Content-Language" content="en-us" />
<?php
echo <<<HEAD
	<meta http-equiv="Content-Type" content="text/html; charset=$charset" />
	<script type="text/javascript" src="tablesort.js" charset="$charset"></script>
HEAD;
?>

	<link rel="stylesheet" type="text/css" href="xcache.css" />
	<title>XCache Administration</title>
</head>

<body>
<h1>XCache Administration</h1>
<span class="switcher"><?php echo switcher("type", $types); ?></span>
<?php
$a = new Cycle('class="col1"', 'class="col2"');
$b = new Cycle('class="col1"', 'class="col2"');
?>
<table cellspacing="1" cellpadding="4" class="cycles">
	<col />
	<col align="right" />
	<col align="right" />
	<col align="right" />
	<col />
	<col />
	<col align="right" />
	<col align="right" />
	<col align="right" />
	<col align="right" />
	<col align="right" />
	<col align="right" />
	<col align="right" />
	<col align="right" />
	<col />
	<tr <?php echo $a->next(); ?>>
		<th></th>
		<th>Slots</th>
		<th>Size</th>
		<th>Avail</th>
		<th>Used</th>
		<th>Clear</th>
		<th>Compiling</th>
		<th>Hits</th>
		<th>Misses</th>
		<th>CLogs</th>
		<th>OOMs</th>
		<th>Protected</th>
		<th>Cached</th>
		<th>Deleted</th>
		<th>Blocks</th>
	</tr>
	<?php
	$numkeys = explode(',', 'slots,size,avail,hits,misses,clogs,ooms,cached,deleted');
	foreach ($cacheinfos as $i => $ci) {
		echo "
		<tr ", $a->next(), ">";
		$ci = number_formats($ci, $numkeys);
		$ci['compiling']    = $ci['type'] == $type_php ? ($ci['compiling'] ? 'yes' : 'no') : '-';
		$ci['can_readonly'] = $ci['can_readonly'] ? 'yes' : 'no';

		$percent = (int) (($ci['size'] - $ci['avail']) / $ci['size'] * 100);
		echo <<<EOS
		<th>{$ci['cache_name']}</th>
		<td>{$ci['slots']}</td>
		<td>{$ci['size']}</td>
		<td>{$ci['avail']}</td>
		<td><div class="percent"><div style="width: $percent%" class="v">&nbsp;</div></div></td>
		<td>
			<form method="post">
				<div>
					<input type="hidden" name="type" value="{$ci['type']}">
					<input type="hidden" name="cacheid" value="{$ci['cacheid']}">
					<input type="submit" name="clearcache" value="Clear" class="submit" onclick="return confirm('Sure to clear?');" />
				</div>
			</form>
		</td>
		<td>{$ci['compiling']}</td>
		<td>{$ci['hits']}</td>
		<td>{$ci['misses']}</td>
		<td>{$ci['clogs']}</td>
		<td>{$ci['ooms']}</td>
		<td>{$ci['can_readonly']}</td>
		<td>{$ci['cached']}</td>
		<td>{$ci['deleted']}</td>
		<td>
EOS;

			$b->reset();
			?>
		</td>
	</tr>
	<?php } ?>
</table>
<?php
foreach ($cacheinfos as $i => $ci) {
?>
<table cellspacing="1" cellpadding="4" class="cycles freeblocks">
	<tr>
		<th><?php echo $ci['cache_name']; ?> size<br>offset</th>
	<?php
	foreach ($ci['free_blocks'] as $block) {
		$size   = number_format($block['size']);
		$offset = number_format($block['offset']);

		$c = $b->next();
		echo "
		<td $c><nobr>$size<br>$offset</nobr></td>";
	}
	?>

	</tr>
</table>
<?php
}
?>
<div style="clear: both">&nbsp;</div>
<?php

if ($cachelist) {
	$isphp = $cachelist['type'] == $type_php;
	if (function_exists("ob_filter_path_nicer")) {
		ob_start("ob_filter_path_nicer");
	}
	foreach (array('Cached' => $cachelist['cache_list'], 'Deleted' => $cachelist['deleted_list']) as $listname => $entries) {
		$a->reset();
		echo "
		<caption>{$cachelist['type_name']} $listname</caption>";
		?>

	<table cellspacing="1" cellpadding="4" class="cycles entrys" width="100%">
		<col />
		<col />
		<col align="right" />
		<col align="right" />
		<col align="right" />
		<col align="right" />
		<col align="right" />
		<col align="right" />
		<?php
		if ($listname == 'Deleted') {
			echo '<col align="right" />';
		}
		if ($isphp) {
			echo '<col align="right" />';
			echo '<col align="right" />';
			echo '<col align="right" />';
		}

		echo "
		<tr ", $a->next(), ">";
		?>

			<th><a href="javascript:" onclick="resort(this); return false">Cache</a></th>
			<th><a href="javascript:" onclick="resort(this); return false">entry</a></th>
			<th><a href="javascript:" onclick="resort(this); return false">Hits</a></th>
			<th><a href="javascript:" onclick="resort(this); return false">Ref count</a></th>
			<th><a href="javascript:" onclick="resort(this); return false">Size</a></th>
			<?php if ($isphp) { ?>
			<th><a href="javascript:" onclick="resort(this); return false">SrcSize</a></th>
			<th><a href="javascript:" onclick="resort(this); return false">Modify</a></th>
			<th><a href="javascript:" onclick="resort(this); return false">device</a></th>
			<th><a href="javascript:" onclick="resort(this); return false">inode</a></th>
			<?php } ?>
			<th><a href="javascript:" onclick="resort(this); return false">Access</a></th>
			<th><a href="javascript:" onclick="resort(this); return false">Create</a></th>
			<?php if ($listname == 'Deleted') { ?>
			<th><a href="javascript:" onclick="resort(this); return false">Delete</a></th>
			<?php } ?>
		</tr>
		<?php
		foreach ($entries as $i => $entry) {
			echo "
			<tr ", $a->next(), ">";
			$name     = htmlspecialchars($entry['name']);
			$hits     = number_format($entry['hits']);
			$refcount = number_format($entry['refcount']);
			$size     = size($entry['size']);
			if ($isphp) {
				$sourcesize = size($entry['sourcesize']);
			}

			if ($isphp) {
				$mtime = age($entry['mtime']);
			}
			$ctime = age($entry['ctime']);
			$atime = age($entry['atime']);
			$dtime = age($entry['dtime']);

			echo <<<ENTRY
			<td>{$entry['cache_name']} {$i}</td>
			<td>{$name}</td>
			<td int="{$entry['hits']}">{$entry['hits']}</td>
			<td int="{$entry['refcount']}">{$entry['refcount']}</td>
			<td int="{$entry['size']}">{$size}</td>
ENTRY;
			if ($isphp) {
				echo <<<ENTRY
				<td int="{$entry['sourcesize']}">{$sourcesize}</td>
				<td int="{$entry['mtime']}">{$mtime}</td>
				<td int="{$entry['device']}">{$entry['device']}</td>
				<td int="{$entry['inode']}">{$entry['inode']}</td>
ENTRY;
			}
			echo <<<ENTRY
			<td int="{$entry['atime']}">{$atime}</td>
			<td int="{$entry['ctime']}">{$ctime}</td>
ENTRY;
			if ($listname == 'Deleted') {
			echo <<<ENTRY
				<td int="{$entry['dtime']}">{$dtime}</td>
ENTRY;
			}

			echo "
		</tr>
			";
		}
		?>

	</table>
<?php
	}
	if (function_exists("ob_filter_path_nicer")) {
		ob_end_flush();
	}
}
?>
</body>
</html>

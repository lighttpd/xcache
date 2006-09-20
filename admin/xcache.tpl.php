<!DOCTYPE HTML PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php
echo <<<HEAD
	<meta http-equiv="Content-Type" content="text/html; charset=$charset" />
	<meta http-equiv="Content-Language" content="$lang" />
	<script type="text/javascript" src="tablesort.js" charset="$charset"></script>
HEAD;
?>

	<link rel="stylesheet" type="text/css" href="xcache.css" />
	<title><?php echo sprintf(_T("XCache %s Administration"), $xcache_version); ?></title>
</head>

<body>
<h1><?php echo sprintf(_T("XCache %s Administration"), $xcache_version); ?></h1>
<a href="help.php" target="_blank" id="help"><?php echo _T("Help") ?> &raquo;</a>
<span class="switcher"><?php echo switcher("type", $types); ?></span>
<?php
$a = new Cycle('class="col1"', 'class="col2"');
$b = new Cycle('class="col1"', 'class="col2"');
?>
<?php echo _T('Caches'); ?>:
<table cellspacing="0" cellpadding="4" class="cycles">
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
		<th>-</th>
		<th><?php echo _T('Slots'); ?></th>
		<th><?php echo _T('Size'); ?></th>
		<th><?php echo _T('Avail'); ?></th>
		<th><?php echo _T('%'); ?></th>
		<th><?php echo _T('Clear'); ?></th>
		<th><?php echo _T('Compiling'); ?></th>
		<th><?php echo _T('Hits'); ?></th>
		<th><?php echo _T('Misses'); ?></th>
		<th><?php echo _T('Clogs'); ?></th>
		<th><?php echo _T('OOMs'); ?></th>
		<th><?php echo _T('Protected'); ?></th>
		<th><?php echo _T('Cached'); ?></th>
		<th><?php echo _T('Deleted'); ?></th>
		<th><?php echo _T('GC'); ?></th>
	</tr>
	<?php
	$numkeys = explode(',', 'slots,size,avail,hits,misses,clogs,ooms,cached,deleted');
	$l_clear = _T('Clear');
	$l_clear_confirm = _T('Sure to clear?');
	foreach ($cacheinfos as $i => $ci) {
		echo "
		<tr ", $a->next(), " height=\"20\">";
		$pavail = (int) ($ci['avail'] / $ci['size'] * 100);
		$pused = 100 - $pavail;

		$ci_slots = size($ci['slots']);
		$ci_size  = size($ci['size']);
		$ci_avail = size($ci['avail']);
		$ci = number_formats($ci, $numkeys);
		$ci['compiling']    = $ci['type'] == $type_php ? ($ci['compiling'] ? 'yes' : 'no') : '-';
		$ci['can_readonly'] = $ci['can_readonly'] ? 'yes' : 'no';
		echo <<<EOS
		<th>{$ci['cache_name']}</th>
		<td title="{$ci['slots']}">{$ci_slots}</td>
		<td title="{$ci['size']}">{$ci_size}</td>
		<td title="{$ci['avail']}">{$ci_avail}</td>
		<td title="{$pavail} %"><div class="percent"><div style="height: {$pused}%" class="pused">&nbsp;</div><div style="height: {$pavail}%" class="pavail">&nbsp;</div></div></td>
		<td>
			<form method="post">
				<div>
					<input type="hidden" name="type" value="{$ci['type']}">
					<input type="hidden" name="cacheid" value="{$ci['cacheid']}">
					<input type="submit" name="clearcache" value="{$l_clear}" class="submit" onclick="return confirm('{$l_clear_confirm}');" />
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
		<td>{$ci['gc']}</td>
EOS;

			$b->reset();
			?>
	</tr>
	<?php } ?>
</table>
<?php echo _T('Free Blocks'); ?>:
<?php
foreach ($cacheinfos as $i => $ci) {
	$b->reset();
?>
<table cellspacing="0" cellpadding="4" class="cycles freeblocks">
	<tr>
		<th><?php echo $ci['cache_name']; ?> <?php echo _T("size"); ?><br><?php echo _T("offset"); ?></th>
	<?php
	foreach ($ci['free_blocks'] as $block) {
		$size   = size($block['size']);
		$offset = size($block['offset']);

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
		<caption>", _T("{$cachelist['type_name']} $listname"), "</caption>";
		?>

	<table cellspacing="0" cellpadding="4" class="cycles entrys" width="100%">
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

			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('Cache'); ?></a></th>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('entry'); ?></a></th>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('Hits'); ?></a></th>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('Refcount'); ?></a></th>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('Size'); ?></a></th>
			<?php if ($isphp) { ?>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('SrcSize'); ?></a></th>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('Modify'); ?></a></th>
			<?php if ($haveinode) { ?>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('device'); ?></a></th>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('inode'); ?></a></th>
			<?php } ?>
			<?php } ?>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('Access'); ?></a></th>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('Create'); ?></a></th>
			<?php if ($listname == 'Deleted') { ?>
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('Delete'); ?></a></th>
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
			if ($listname == 'Deleted') {
				$dtime = age($entry['dtime']);
			}

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
ENTRY;
				if (isset($entry['inode'])) {
					echo <<<ENTRY
					<td int="{$entry['device']}">{$entry['device']}</td>
					<td int="{$entry['inode']}">{$entry['inode']}</td>
ENTRY;
				}
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
if ($moduleinfo) {
	$t_moduleinfo = _T("Module Info");
	echo <<<HTML
<h2>$t_moduleinfo</h2>
<div class="moduleinfo">$moduleinfo</div>
HTML;
}
?>
<div class="footnote">
<?php echo <<<EOS
Powered By: XCache {$xcache_version}, {$xcache_modules}
EOS;
?>
</div>

</body>
</html>

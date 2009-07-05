<?php include("header.tpl.php"); ?>
<div id="help">
	<a href="help.php"><?php echo _T("Help") ?> &raquo;</a>
</div>
<div class="switcher"><?php echo switcher("type", $types); ?></div>
<?php
$a = new Cycle('class="col1"', 'class="col2"');
$b = new Cycle('class="col1"', 'class="col2"');
?>
<table cellspacing="0" cellpadding="4" class="cycles">
	<caption><?php echo _T('Caches'); ?></caption>
	<col />
	<col align="right" />
	<col align="right" />
	<col align="right" />
	<col />
	<col />
	<col align="right" />
	<col align="right" />
	<col align="right" />
	<col />
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
		<th><?php echo _T(isset($free_graph_width) ? '% Free' : '% Used'); ?></th>
		<th><?php echo _T('Clear'); ?></th>
		<th><?php echo _T('Compiling'); ?></th>
		<th><?php echo _T('Hits'); ?></th>
		<th><?php echo _T('Hits/H'); ?></th>
		<th><?php echo _T('Hits 24H'); ?></th>
		<th><?php echo _T('Hits/S'); ?></th>
		<th><?php echo _T('Misses'); ?></th>
		<th><?php echo _T('Clogs'); ?></th>
		<th><?php echo _T('OOMs'); ?></th>
		<th><?php echo _T('Errors'); ?></th>
		<th><?php echo _T('Protected'); ?></th>
		<th><?php echo _T('Cached'); ?></th>
		<th><?php echo _T('Deleted'); ?></th>
		<th><?php echo _T('GC'); ?></th>
	</tr>
	<?php
	$numkeys = explode(',', 'slots,size,avail,hits,misses,clogs,ooms,errors,cached,deleted');
	$l_clear = _T('Clear');
	$l_clear_confirm = _T('Sure to clear?');
	foreach ($cacheinfos as $i => $ci) {
		echo "
		<tr ", $a->next(), ">";
		$pvalue = (int) ($ci['avail'] / $ci['size'] * 100);
		$pempty = 100 - $pvalue;
		if (!isset($free_graph_width)) {
			// swap
			$tmp = $pvalue;
			$pvalue = $pempty;
			$pempty = $tmp;
		}

		$w = $graph_width;
		if (empty($ci['istotal'])) {
			$graph = freeblock_to_graph($ci['free_blocks'], $ci['size']);
			$blocksgraph = "<div class=\"blocksgraph\" style=\"width: {$w}px\">{$graph}</div>";
		}
		else {
			$blocksgraph = '';
		}

		$ci_slots = size($ci['slots']);
		$ci_size  = size($ci['size']);
		$ci_avail = size($ci['avail']);
		$ci = number_formats($ci, $numkeys);

		$hits_avg_h     = number_format(array_avg($ci['hits_by_hour']), 2);
		$hits_avg_s     = number_format(array_avg($ci['hits_by_second']), 2);
		$hits_graph_h   = hits_to_graph($ci['hits_by_hour']);
		$hits_graph_h_w = count($ci['hits_by_hour']) * 2;

		if (!empty($ci['istotal'])) {
			$ci['compiling']    = '-';
			$ci['can_readonly'] = '-';
		}
		else {
			$ci['compiling']    = $ci['type'] == $type_php ? ($ci['compiling'] ? 'yes' : 'no') : '-';
			$ci['can_readonly'] = $ci['can_readonly'] ? 'yes' : 'no';
		}
		echo <<<EOS
		<th>{$ci['cache_name']}</th>
		<td title="{$ci['slots']}">{$ci_slots}</td>
		<td title="{$ci['size']}">{$ci_size}</td>
		<td title="{$ci['avail']}">{$ci_avail}</td>
		<td title="{$pvalue} %"
			><div class="percent" style="width: {$w}px"
				><div style="width: {$pvalue}%" class="pvalue"></div
				><div style="width: {$pempty}%" class="pempty"></div
			></div
		>{$blocksgraph}</td>
		<td
			><form method="post" action=""
				><div
					><input type="hidden" name="type" value="{$ci['type']}"
					/><input type="hidden" name="cacheid" value="{$ci['cacheid']}"
					/><input type="submit" name="clearcache" value="{$l_clear}" class="submit" onclick="return confirm('{$l_clear_confirm}');"
				/></div
			></form
		></td>
		<td>{$ci['compiling']}</td>
		<td>{$ci['hits']}</td>
		<td>{$hits_avg_h}</td>
		<td><div class="hitsgraph" style="width: {$hits_graph_h_w}px">{$hits_graph_h}</div></td>
		<td>{$hits_avg_s}</td>
		<td>{$ci['misses']}</td>
		<td>{$ci['clogs']}</td>
		<td>{$ci['ooms']}</td>
		<td>{$ci['errors']}</td>
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
<div class="blockarea legends">
	<div class="legendtitle"><?php echo _T('Legends:'); ?></div>
	<div class="legend pvalue">&nbsp;&nbsp;</div>
	<div class="legendtitle"><?php echo _T(isset($free_graph_width) ? '% Free' : '% Used'); ?></div>
	<div class="legend" style="background: rgb(0,0,255)">&nbsp;&nbsp;</div>
	<div class="legendtitle"><?php echo _T(isset($free_graph_width) ? 'Free Blocks' : 'Used Blocks'); ?></div>
	<div class="legend" style="background: rgb(255,0,0)">&nbsp;&nbsp;</div>
	<div class="legendtitle"><?php echo _T('Hits'); ?></div>
</div>
<?php

if ($cachelist) {
	$isphp = $cachelist['type'] == $type_php;
	if (function_exists("ob_filter_path_nicer")) {
		ob_start("ob_filter_path_nicer");
	}
	foreach (array('Cached' => $cachelist['cache_list'], 'Deleted' => $cachelist['deleted_list']) as $listname => $entries) {
		$a->reset();
		?>

	<form action="" method="post">
	<table cellspacing="0" cellpadding="4" class="cycles entries" width="100%">
		<caption><?php echo _T("{$cachelist['type_name']} $listname"); ?></caption>
		<col />
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

			<?php if (!$isphp) { ?>
			<th width="20">R</th>
			<?php } ?>
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
			<th><a href="javascript:" onclick="resort(this); return false"><?php echo _T('hash'); ?></a></th>
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

			if (!$isphp) {
				echo <<<ENTRY
					<td><input type="checkbox" name="remove[]" value="{$name}"/></td>
ENTRY;
				$uname = urlencode($entry['name']);
				$namelink = "<a href=\"edit.php?name=$uname\">$name</a>";
			}
			else {
				$namelink = $name;
			}

			echo <<<ENTRY
			<td>{$entry['cache_name']} {$i}</td>
			<td>{$namelink}</td>
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
			<td int="{$entry['hvalue']}">{$entry['hvalue']}</td>
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
	<input type="submit" value="<?php echo _T("Remove Selected"); ?>">
	</form>
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
<?php include("footer.tpl.php"); ?>

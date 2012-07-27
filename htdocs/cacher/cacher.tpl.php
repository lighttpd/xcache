<?php include("../common/header.tpl.php"); ?>
<div class="switcher"><?php echo switcher("type", $listTypes); ?></div>
<div id="help">
	<a href="help.php"><?php echo _("Help") ?> &raquo;</a>
</div>
<?php
$a = new Cycle('class="col1"', 'class="col2"');
$b = new Cycle('class="col1"', 'class="col2"');
?>
<table cellspacing="0" cellpadding="4" class="cycles caches">
	<caption><?php echo _('Caches'); ?></caption>
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
	<?php echo
		th(N_("cache.cache"))
		, th(N_("cache.size"))
		, th(N_("cache.avail"))
		, th(N_("cache.used"))
		, th(N_("cache.blocksgraph"))
		, th(N_("cache.operations"))
		, th(N_("cache.compiling"))
		, th(N_("cache.hits"))
		, th(N_("cache.hits_graph"))
		, th(N_("cache.hits_avg_h"))
		, th(N_("cache.hits_avg_s"))
		, th(N_("cache.updates"))
		, th(N_("cache.clogs"))
		, th(N_("cache.ooms"))
		, th(N_("cache.errors"))
		, th(N_("cache.readonly_protected"))
		, th(N_("cache.cached"))
		, th(N_("cache.deleted"))
		, th(N_("cache.gc_timer"))
		;
	?>
	</tr>
	<?php
	$numkeys = explode(',', 'slots,size,avail,hits,updates,clogs,ooms,errors,cached,deleted');
	$l_clear = _('Clear');
	$l_clear_confirm = _('Sure to clear?');
	foreach ($cacheinfos as $i => $ci) {
		echo "
		<tr ", $a->next(), ">";
		$pvalue = (int) ($ci['avail'] / $ci['size'] * 100);
		$pempty = 100 - $pvalue;
		if ($config['percent_graph_type'] == 'used') {
			// swap
			$tmp = $pvalue;
			$pvalue = $pempty;
			$pempty = $tmp;
		}

		$w = $config['percent_graph_width'];
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
		<td><div class="hitsgraph" style="width: {$hits_graph_h_w}px">{$hits_graph_h}</div></td>
		<td>{$hits_avg_h}</td>
		<td>{$hits_avg_s}</td>
		<td>{$ci['updates']}</td>
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
	<div class="legendtitle"><?php echo _('Legends:'); ?></div>
	<div class="legend pvalue">&nbsp;&nbsp;</div>
	<div class="legendtitle"><?php echo _($config['percent_graph_type'] == 'free' ? '% Free' : '% Used'); ?></div>
	<div class="legend" style="background: rgb(0,0,255)">&nbsp;&nbsp;</div>
	<div class="legendtitle"><?php echo _($config['percent_graph_type'] == 'free' ? 'Free Blocks' : 'Used Blocks'); ?></div>
	<div class="legend" style="background: rgb(255,0,0)">&nbsp;&nbsp;</div>
	<div class="legendtitle"><?php echo _('Hits'); ?></div>
</div>
<?php

if ($cachelist) {
	$isphp = $cachelist['type'] == $type_php;
	ob_start($config['path_nicer']);
	foreach (array('Cached' => $cachelist['cache_list'], 'Deleted' => $cachelist['deleted_list']) as $listname => $entries) {
		$a->reset();
		?>

	<form action="" method="post">
	<table cellspacing="0" cellpadding="4" class="cycles entries" width="100%">
		<caption><?php echo _("{$cachelist['type_name']} $listname"); ?></caption>
		<?php
		echo "
		<tr ", $a->next(), ">";
		?>

			<?php
			if ($isphp) {
				echo
					th("entry.id")
					;
			}
			else {
				echo
					th("entry.remove", 'width="20"')
					;
			}

			echo
				th(N_("entry.name"))
				, th(N_("entry.hits"))
				, th(N_("entry.size"))
				;

			if ($isphp) {
				echo
					th(N_("entry.refcount"))
					, th(N_("entry.phprefcount"))
					, th(N_("entry.class_cnt"))
					, th(N_("entry.function_cnt"))
					, th(N_("entry.file_size"))
					, th(N_("entry.file_mtime"))
					;
				if ($haveinode) {
					echo
						th(N_("entry.file_device"))
						, th(N_("entry.file_inode"))
						;
				}
			}
			echo
				th(N_("entry.hash"))
				, th(N_("entry.atime"))
				, th(N_("entry.ctime"))
				;

			if ($listname == 'Deleted') {
				echo
					th(N_("entry.delete"))
					;
			}
			?>
		</tr>
		<?php
		foreach ($entries as $i => $entry) {
			echo "
			<tr ", $a->next(), ">";
			$name     = htmlspecialchars($entry['name']);
			$hits     = number_format($entry['hits']);
			$size     = size($entry['size']);
			if ($isphp) {
				$class_cnt    = number_format($entry['class_cnt']);
				$function_cnt = number_format($entry['function_cnt']);
				$phprefcount  = number_format($entry['phprefcount']);
				$file_size    = size($entry['file_size']);
			}

			if ($isphp) {
				$file_mtime = age($entry['file_mtime']);
			}
			$ctime = age($entry['ctime']);
			$atime = age($entry['atime']);
			if ($listname == 'Deleted') {
				$dtime = age($entry['dtime']);
			}

			if ($isphp) {
				$namelink = $name;
				echo <<<ENTRY
				<td>{$entry['cache_name']} {$i}</td>
ENTRY;
			}
			else {
				echo <<<ENTRY
					<td><label><input type="checkbox" name="remove[]" value="{$name}"/>{$entry['cache_name']} {$i}</label></td>
ENTRY;
				$uname = urlencode($entry['name']);
				$namelink = "<a href=\"edit.php?name=$uname\">$name</a>";
			}

			echo <<<ENTRY
			<td>{$namelink}</td>
			<td align="right" int="{$entry['hits']}">{$entry['hits']}</td>
			<td align="right" int="{$entry['size']}">{$size}</td>
ENTRY;
			if ($isphp) {
				$refcount = number_format($entry['refcount']);
				echo <<<ENTRY
				<td align="right" int="{$entry['refcount']}">{$entry['refcount']}</td>
				<td align="right" int="{$entry['phprefcount']}">{$phprefcount}</td>
				<td align="right" int="{$entry['class_cnt']}">{$class_cnt}</td>
				<td align="right" int="{$entry['function_cnt']}">{$function_cnt}</td>
				<td align="right" int="{$entry['file_size']}">{$file_size}</td>
				<td align="right" int="{$entry['file_mtime']}">{$file_mtime}</td>
ENTRY;
				if (isset($entry['file_inode'])) {
					echo <<<ENTRY
					<td align="right" int="{$entry['file_device']}">{$entry['file_device']}</td>
					<td align="right" int="{$entry['file_inode']}">{$entry['file_inode']}</td>
ENTRY;
				}
			}
			echo <<<ENTRY
			<td align="right" int="{$entry['hvalue']}">{$entry['hvalue']}</td>
			<td align="right" int="{$entry['atime']}">{$atime}</td>
			<td align="right" int="{$entry['ctime']}">{$ctime}</td>
ENTRY;
			if ($listname == 'Deleted') {
			echo <<<ENTRY
				<td align="right" int="{$entry['dtime']}">{$dtime}</td>
ENTRY;
			}

			echo "
		</tr>
			";
		}
		?>

	</table>
	<?php if (!$isphp) { ?>
	<input type="submit" value="<?php echo _("Remove Selected"); ?>">
	<?php } ?>
	</form>
<?php
	}
	ob_end_flush();
}
if ($moduleinfo) {
	$t_moduleinfo = _("Module Info");
	echo <<<HTML
<h2>$t_moduleinfo</h2>
<div class="phpinfo">$moduleinfo</div>
HTML;
}
?>
<?php include("../common/footer.tpl.php"); ?>

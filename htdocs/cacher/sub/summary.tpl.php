<?php $cycleClass = new Cycle('class="col1"', 'class="col2"'); ?>
<table cellspacing="0" cellpadding="4" class="cycles caches">
	<caption><?php echo _('Caches'); ?></caption>
	<tr>
	<?php echo
		th(N_("cache.cache"))
		, th(N_("cache.size"))
		, th(N_("cache.avail"))
		, th(N_("cache.used"))
		, th(N_("cache.blocksgraph"))
		, th(N_("cache.operations"))
		, th(N_("cache.status"))
		, th(N_("cache.hits"))
		, th(N_("cache.hits_graph"))
		, th(N_("cache.hits_avg_h"))
		, th(N_("cache.hits_avg_s"))
		, th(N_("cache.updates"))
		, th(N_("cache.skips"))
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
	$numkeys = explode(',', 'slots,size,avail,hits,updates,skips,ooms,errors,cached,deleted');
	$l_clear = _('Clear');
	$l_disabled = _('Disabled');
	$l_disable = _('Disable');
	$l_enable = _('Enable');
	$l_compiling = _('Compiling');
	$l_normal = _('Normal');
	$l_confirm = _('Sure?');
	foreach ($cacheinfos as $i => $ci) {
		$class = $cycleClass->next();
		echo <<<TR
	<tr {$class}>

TR;
		$pvalue = (int) ($ci['avail'] / $ci['size'] * 100);
		$pempty = 100 - $pvalue;
		if ($config['percent_graph_type'] == 'used') {
			// swap
			$tmp = $pvalue;
			$pvalue = $pempty;
			$pempty = $tmp;
		}

		$w = $config['percent_graph_width'] + 2;
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
		$hits_graph_h   = get_cache_hits_graph($ci, 'hits_by_hour');
		$hits_graph_h_w = count($ci['hits_by_hour']) * 2;

		if (!empty($ci['istotal'])) {
			$ci['status']       = '-';
			$ci['can_readonly'] = '-';
		}
		else {
			if ($ci['disabled']) {
				$ci['status'] = $l_disabled
					. sprintf("(%s)", age($ci['disabled']));
			}
			else if ($ci['type'] == XC_TYPE_PHP) {
				$ci['status'] = $ci['compiling']
					? $l_compiling . sprintf("(%s)", age($ci['compiling']))
					: $l_normal;
			}
			else {
				$ci['status'] = '-';
			}
			$ci['can_readonly'] = $ci['can_readonly'] ? 'yes' : 'no';
		}
		$enabledisable = $ci['disabled'] ? 'enable' : 'disable';
		$l_enabledisable = $ci['disabled'] ? $l_enable : $l_disable;
		echo <<<EOS
		<th>{$ci['cache_name']}</th>
		<td align="right" title="{$ci['slots']}">{$ci_slots}</td>
		<td align="right" title="{$ci['size']}">{$ci_size}</td>
		<td align="right" title="{$ci['avail']}">{$ci_avail}</td>
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
					/><input type="submit" name="clearcache" value="{$l_clear}" class="submit" onclick="return confirm('{$l_confirm}');"
					/><input type="submit" name="{$enabledisable}" value="{$l_enabledisable}" class="submit"
				/></div
			></form
		></td>
		<td>{$ci['status']}</td>
		<td align="right">{$ci['hits']}</td>
		<td><div class="hitsgraph" style="width: {$hits_graph_h_w}px">{$hits_graph_h}</div></td>
		<td align="right">{$hits_avg_h}</td>
		<td align="right">{$hits_avg_s}</td>
		<td align="right">{$ci['updates']}</td>
		<td align="right">{$ci['skips']}</td>
		<td align="right">{$ci['ooms']}</td>
		<td align="right">{$ci['errors']}</td>
		<td>{$ci['can_readonly']}</td>
		<td align="right">{$ci['cached']}</td>
		<td align="right">{$ci['deleted']}</td>
		<td align="right">{$ci['gc']}</td>

EOS;
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
<?php unset($cycleClass); ?>

<!DOCTYPE HTML PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
	<meta http-equiv="Content-Language" content="en-us" />
<?php
echo <<<HEAD
	<meta http-equiv="Content-Type" content="text/html; charset=$this->charset" />
	<meta http-equiv="Content-Language" content="{$this->lang}" />
	<script type="text/javascript" src="tablesort.js" charset="$this->charset"></script>
HEAD;
?>

	<link rel="stylesheet" type="text/css" href="coverager.css" />
	<title><?php echo _T("XCache PHP Code Coverage Viewer"); ?></title>
</head>
<body>
<h1><?php echo _T("XCache PHP Code Coverage Viewer"); ?></h1>

<?php
function calc_percent($info, &$percent, &$class)
{
	if (!$info['total']) {
		$percent = 0;
	}
	else {
		$percent = (int) ($info['hits'] / $info['total'] * 100);
	}
	if ($percent < 15) {
		$class = "Lo";
	}
	else if ($percent < 50) {
		$class = "Med";
	}
	else {
		$class = "Hi";
	}
}

function bar($percent, $class)
{
	return <<<EOS
	<div class="coverBarOutline">
		<div class="coverBar{$class}" style="width:{$percent}%"></div>
		<div class="coverPer{$class}">{$percent}</div>
	</div>
EOS;
}

function dir_head()
{
	global $cycle;
	$cycle = new Cycle('class="col1"', 'class="col2"');
	$l_dir = _T("Directory");
	$l_per = _T("Percent");
	$l_hit = _T("Hits");
	$l_lns = _T("Lines");
	$l_tds = _T("TODO");
	return <<<EOS
	<table align="center" cellpadding="2" cellspacing="0" border="0" class="cycles">
	<tr>
		<th>{$l_dir}</th><th>{$l_per}</th><th>{$l_hit}</th><th>{$l_lns}</th><th>{$l_tds}</th>
	</tr>
EOS;
}

function dir_row($info, $srcdir)
{
	global $cycle;
	if ($info['files'] || $info['todos']) {
		$srcdir .= DIRECTORY_SEPARATOR;
		$c = $cycle->next();
		$srcdir_html = htmlspecialchars($srcdir);
		$todos = number_format($info['todos']);
		if ($info['total']) {
			$srcdir_url = urlencode($srcdir);
			$hits  = number_format($info['hits']);
			$total = number_format($info['total']);
			calc_percent($info, $percent, $class);
			$bar = bar($percent, $class);
			return <<<EOS
			<tr $c>
				<td class="coverFile"><a href="?path={$srcdir_url}">{$srcdir_html}</a></td>
				<td class="coverBar">$bar</td>
				<td class="coverNum{$class}">{$hits}</td>
				<td class="coverNum{$class}">{$total}</td>
				<td class="coverNum{$class}">{$todos}</td>
			</tr>
EOS;
		}
		else {
			return <<<EOS
			<tr $c>
				<td class="coverFile">{$srcdir_html}</td>
				<td class="coverBar"></td>
				<td class="coverNumLo"></td>
				<td class="coverNumLo"></td>
				<td class="coverNumLo">{$todos}</td>
			</tr>
EOS;
		}
	}
}

function dir_foot()
{
	return <<<EOS
	</table>
EOS;
}

function file_head()
{
	global $cycle;
	$cycle = new Cycle('class="col1"', 'class="col2"');
	$l_fil = _T("File");
	$l_per = _T("Percent");
	$l_hit = _T("Hits");
	$l_lns = _T("Lines");
	return <<<EOS
	<br>
	<table align="center" cellpadding="2" cellspacing="0" border="0" class="cycles">
	<tr>
		<th>{$l_fil}</th><th>{$l_per}</th><th>{$l_hit}</th><th>{$l_lns}</th>
	</tr>
EOS;
}

function file_row($info, $srcfile)
{
	global $cycle;

	$c = $cycle->next();
	$srcfile_html = htmlspecialchars($srcfile);
	$total = number_format($info['total']);
	if ($info['total']) {
		$hits = number_format($info['hits']);
		$srcfile_url = urlencode($srcfile);
		calc_percent($info, $percent, $class);
		$bar = bar($percent, $class);
		return <<<EOS
			<tr $c>
					<td class="coverFile"><a href="?path={$srcfile_url}">{$srcfile_html}</a></td>
					<td class="coverBar">$bar</td>
					<td class="coverNum{$class}">{$hits}</td>
					<td class="coverNum{$class}">{$total}</td>
			</tr>
EOS;
	}
	else {
		return <<<EOS
			<tr $c>
					<td class="coverFile">{$srcfile_html}</a></td>
					<td class="coverBar"></td>
					<td class="coverNumLo"></td>
					<td class="coverNumLo">{$total}</td>
			</tr>
EOS;
	}
}

function file_foot()
{
	return <<<EOS
    </table>
EOS;
}

$l_root = _T("root");
if ($action == 'dir') {
	if (function_exists('ob_filter_path_nicer')) {
		ob_start('ob_filter_path_nicer');
	}
	$path_html = htmlspecialchars($path);
	echo <<<EOS
	<a href="?">$l_root</a> $path<br />
EOS;
	echo dir_head($dirinfo);
	echo dir_row($dirinfo, $path);
	echo dir_foot($dirinfo);
	if ($dirinfo['subdirs']) {
		echo dir_head();
		foreach ($dirinfo['subdirs'] as $srcdir => $info) {
			echo dir_row($info, $srcdir);
		}
		echo dir_foot();
	}
	if ($dirinfo['files']) {
		echo file_head();
		foreach ($dirinfo['files'] as $srcfile => $info) {
			echo file_row($info, $srcfile);
		}
		echo file_foot();
	}
}
else if ($action == 'file') {
	if (function_exists('ob_filter_path_nicer')) {
		ob_start('ob_filter_path_nicer');
	}
	$dir_url = urlencode($dir);
	$dir_html = htmlspecialchars($dir);
	echo <<<EOS
	<a href="?">$l_root</a> <a href="?path={$dir_url}">{$dir_html}</a>/<b>{$filename}</b><br />
EOS;

	echo file_head();
	echo file_row($fileinfo, $path);
	echo file_foot();

	if ($tplfile) {
		$tplfile_html = htmlspecialchars($tplfile);
		echo <<<EOS
		<a href="#tpl">{$tplfile_html}</a><br />
EOS;
	}
	if (function_exists('ob_filter_path_nicer')) {
		ob_end_flush();
	}
	echo <<<EOS
	<pre class="code"><ol>{$filecov}</ol></pre>
EOS;
	if ($tplfile) {
		echo <<<EOS
	<a name="tpl">{$tplfile}</a>
	<pre class="code"><ol>{$tplcov}</ol></pre>
EOS;
	}
}
else {
	echo htmlspecialchars($action);
}
?>

<div class="footnote">
Powered By: XCache <?php echo $xcache_version; ?> coverager <?php echo _T("module"); ?>
</div>

</body>
</html>

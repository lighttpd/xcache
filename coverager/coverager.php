<?php

include("./common.php");

class Cycle
{
	var $values;
	var $i;
	var $count;

	function Cycle($v)
	{
		$this->values = func_get_args();
		$this->i = -1;
		$this->count = count($this->values);
	}

	function next()
	{
		$this->i = ($this->i + 1) % $this->count;
		return $this->values[$this->i];
	}

	function cur()
	{
		return $this->values[$this->i];
	}

	function reset()
	{
		$this->i = -1;
	}
}

class XcacheCoverageViewer
{
	var $syntaxhiglight = true;
	var $usecache = false;
	var $include_paths = array();
	var $exclude_paths = array();
	var $charset = 'UTF-8';
	var $lang = 'en-us';
	var $datadir = null;
	var $datadir_len = null;
	var $path = null;
	var $outpath = null;

	function XcacheCoverageViewer()
	{
		$this->datadir = ini_get('xcache.coveragedump_directory');

		// copy config
		foreach (array('charset', 'include_paths', 'exclude_paths', 'syntaxhiglight', 'usecache', 'datadir', 'lang') as $k) {
			if (isset($GLOBALS[$k])) {
				$this->{$k} = $GLOBALS[$k];
			}
		}

		$this->datadir = preg_replace('!/$!', '', $this->datadir);
		$this->datadir_len = strlen($this->datadir);

		$this->path = isset($_GET['path']) ? $_GET['path'] : '';
		$this->path = preg_replace('!\.{2,}!', '.', $this->path);
		$qsep = preg_quote(DIRECTORY_SEPARATOR, '!');
		$this->path = preg_replace("![\\\\$qsep]{2,}!", DIRECTORY_SEPARATOR, $this->path);
		$this->path = preg_replace("!$qsep$!", '', $this->path);
		if ($this->path == '/') {
			$this->path = '';
		}
		$this->outpath = $this->datadir . $this->path;
	}

	function main()
	{
		$path = $this->path;

		if (is_dir($this->outpath)) {
			$action = 'dir';
			$prefix_len = strlen($path) + 1;
			$dirinfo = $this->loadDir($this->outpath);
			if (!$this->usecache) {
				ksort($dirinfo['subdirs']);
				ksort($dirinfo['files']);
			}
		}
		else if (is_file($this->outpath . ".pcov")) {
			$action = 'file';

			$dir = dirname($path);
			$filename = basename($path);

			$fileinfo = $this->loadCov($this->outpath . ".pcov");

			$lines = file($path);
			// fix the tabs not in the middle
			foreach ($lines as $l => $line) {
				if (preg_match('!^(\\t*)([^\\t]+\\t.*)$!s', $line, $m)) {
					$lines[$l] = $m[1];
					$chunks = explode("\t", $m[2]);
					for ($i = 0, $c = count($chunks) - 1; $i < $c; $i ++) {
						$lines[$l] .= $chunks[$i] . str_repeat(" ", 4 - (strlen($chunks[$i]) % 4));
					}
					$lines[$l] .= $chunks[$c];
				}
			}
			if ($this->syntaxhiglight) {
				$source = implode('', $lines);
				ob_start();
				highlight_string($source);
				$lines = explode('<br />', str_replace("\n", "", ob_get_clean()));
				$last = array_pop($lines);
				$filecov = sprint_cov($fileinfo['cov'], $lines, false);
				$filecov .= $last;
				unset($source);
			}
			else {
				$filecov = sprint_cov($fileinfo['cov'], $lines);
			}

			list($tplfile, $tpllines, $tplcov) = $this->loadTplCov($fileinfo['cov'], substr($this->outpath, $this->datadir_len));
			if ($tplfile) {
				$tplcov = sprint_cov($tplinfo['tplcov'], $tpllines);
				unset($tpllines);
			}
		}
		else if (!$this->datadir) {
			$action = 'require xcache.coveragedump_directory';
		}
		else {
			$action = "no data";
		}

		$xcache_version = XCACHE_VERSION;
		include("coverager.tpl.php");
	}

	function loadDir($outdir, $addtodo = null)
	{
		if ($this->usecache) {
			$cachefile = $outdir . "/.pcovcache";
			if (file_exists($cachefile)) {
				return unserialize(file_get_contents($cachefile));
			}
		}
		$srcdir = substr($outdir, $this->datadir_len);

		$total = $hits = $todos = 0;
		$files = array();
		$subdirs = array();
		if (!isset($addtodo)) {
			if ($this->include_paths) {
				foreach ($this->include_paths as $p) {
					if (strncmp($p, $srcdir, strlen($p)) == 0) {
						$addtodo = true;
						break;
					}
				}
			}
		}
		if ($addtodo) {
			if ($this->exclude_paths) {
				foreach ($this->exclude_paths as $p) {
					if (strncmp($p, $srcdir, strlen($p)) == 0) {
						$addtodo = false;
						break;
					}
				}
			}
		}
		foreach (glob($outdir . "/*") as $outfile) {
			if (is_dir($outfile)) {
				$info = $this->loadDir($outfile, $addtodo);
				$srcfile = substr($outfile, $this->datadir_len);
				$subdirs += $info['subdirs'];
				$total   += $info['total'];
				$hits    += $info['hits'];
				$todos   += $info['todos'];
				unset($info['subdirs']);
				$subdirs[$srcfile] = $info;
			}
			else if (substr($outfile, -5) == ".pcov") {
				// pass
				$info = $this->loadFile($outfile);
				$total += $info['total'];
				$hits  += $info['hits'];
				$srcfile = substr($outfile, $this->datadir_len, -5);
				$files[$srcfile] = $info;
			}
			else {
				continue;
			}
		}
		if ($addtodo === true) {
			foreach (glob($srcdir . "/*") as $srcfile) {
				if (!isset($files[$srcfile]) && is_file($srcfile)) {
					$files[$srcfile] = array('total' => 0, 'hits' => 0);
					$todos ++;
				}
				else if (!isset($subdirs[$srcfile]) && is_dir($srcfile)) {
					$subdirs[$srcfile] = array('total' => 0, 'hits' => 0, 'todos' => 1, 'files' => 0, 'subdirs' => array());
					$todos ++;
				}
			}
		}

		if ($this->usecache) {
			ksort($subdirs);
			ksort($files);
		}

		$info = array(
				'total'   => $total,
				'hits'    => $hits,
				'todos'   => $todos,
				'files'   => $files,
				'subdirs' => $subdirs,
				);

		if ($this->usecache) {
			$fp = fopen($cachefile, "wb");
			fwrite($fp, serialize($info));
			fclose($fp);
		}
		return $info;
	}

	function loadFile($file)
	{
		if ($this->usecache) {
			$cachefile = $file . "cache";
			if (file_exists($cachefile)) {
				return unserialize(file_get_contents($cachefile));
			}
		}

		$info = $this->loadCov($file); //, $lines);
		unset($info['cov']);

		if ($this->usecache) {
			$fp = fopen($cachefile, "wb");
			fwrite($fp, serialize($info));
			fclose($fp);
		}
		return $info;
	}

	function loadCov($file)//, $lines)
	{
		$total = $hits = 0;

		$cov = xcache_coverager_decode(file_get_contents($file));

		return array('total' => count($cov) - 1, 'hits' => $cov[0], 'cov' => $cov);
	}

	function loadTplCov($cov, $ctpl)
	{
		$tplinfofile = $ctpl . '.phpinfo';

		if (!file_exists($tplinfofile)) {
			return;
		}

		$tplinfo = unserialize(file_get_contents($tplinfofile));

		if (!isset($tplinfo['sourceFile'])) {
			return;
		}
		$tplfile = $tplinfo['sourceFile'];
		if (!isset($tplinfo['lineMap']) || !count($tplinfo['lineMap'])) {
			return;
		}

		$tpllines = file($tplfile);

		$dline = 0;
		$sline = 0;
		$tplcov = array();
		foreach ($cov as $line => $times) {
			// find nearest line
			while ($dline < $line) {
				if ((list($dline, $sline) = each($tplinfo['lineMap'])) === false) {
					break 2;
				}
			}

			$tplcov[$sline] = $times;
		}
		return array($tplfile, $tpllines, $tplcov);
	}
}

function sprint_cov($cov, $lines, $encode = true)
{
	foreach ($lines as $l => $line) {
		$offs = $l + 1;
		if ($encode) {
			$line = str_replace("\n", "", htmlspecialchars($line));
		}
		if (isset($cov[$offs])) {
			$lines[$l] = sprintf("<li class=\"line%sCov\"> %s\t%s</li>\n"
					, $cov[$offs] ? '' : 'No'
					, $cov[$offs]
					, $line);
		}
		else {
			$lines[$l] = "<li>\t$line</li>\n";
		}
	}
	return implode('', $lines);
}

$app = new XcacheCoverageViewer();
$app->main();

?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
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

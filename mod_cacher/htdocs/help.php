<?php
include("./common.php");
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php
echo <<<HEAD
	<meta http-equiv="Content-Type" content="text/html; charset=$config[charset]" />
	<meta http-equiv="Content-Language" content="$config[lang]" />
	<script type="text/javascript" src="tablesort.js" charset="$config[charset]"></script>
HEAD;
?>

	<link rel="stylesheet" type="text/css" href="xcache.css" />
	<title><?php echo _('XCache Help'); ?></title>
	<script>
	function toggle(o)
	{
		o.style.display = o.style.display != 'block' ? 'block' : 'none';
	}
	</script>
</head>

<body>
<h1><?php echo _('XCache Help'); ?></h1>
<div id1="help">
<?php include(get_language_file("help")); ?>
</div>

<?php echo _('See also'); ?>: <a href="http://xcache.lighttpd.net/wiki/PhpIni">Setting php.ini for XCache</a> @ <a href="http://xcache.lighttpd.net/">XCache wiki</a>
</body>
</html>

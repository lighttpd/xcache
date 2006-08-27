<?php
$charset = "UTF-8";
if (file_exists("./config.php")) {
	include("./config.php");
}
?>
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
	<title>XCache Administration Help</title>
	<script>
	function toggle(o)
	{
		o.style.display = o.style.display != 'block' ? 'block' : 'none';
	}
	</script>
</head>

<body>
<h1>XCache Administration Help</h1>
<a href="javascript:" onclick="toggle(document.getElementById('help'));return false" style="display:block; float: right">Help</a>
<div id1="help">
<h2>Cache Legends</h2>
<pre>
<b>Slots:      </b>Number of hash slots. the setting from your php.ini
<b>Size:       </b>Cache Size, Size of the cache (or cache chunk), in bytes
<b>Avail:      </b>Available Memory, free memory in bytes of this cache
<b>Used:       </b>Used Percent, A bar shows how much memory used in percent
<b>Clear:      </b>Clear Button, Press the button to clean this cache
<b>Compiling:  </b>Compiling flag, "yes" if the cache is busy compiling php script
<b>Hits:       </b>Cache Hits, hit=a var/php is loaded from this cache
<b>Misses:     </b>Cache Misses, miss=a var/php is requested but not in the cache
<b>Clogs:      </b>Compiling Clogs, clog=compiling is needed but avoided to wait(be blocked)
            when the cache is busy compiling already
<b>OOMs:       </b>Out Of Memory, how many times a new item should be stored but there isn't
            enough memory in the cache, think of increasing the xcache.size or xcache.var_size
<b>Protected:  </b>Whether readonly_protection is available and enable on this cache
<b>Cached:     </b>Number of entries stored in this cache
<b>Deleted:    </b>Number of entries is pending in delete list (expired but referenced)
<b>Free Blocks:</b>Free blocks list in the specified cache
</pre>

<h2>List Legends</h2>
<pre>
<b>entry:      </b>The entry name or filename
<b>Hits:       </b>Times this entry is hit (loaded from this cache)
<b>Refcount:   </b>Reference count this entry is holded by a php request
<b>Size:       </b>Size in bytes of this entry in the cache
<b>SrcSize:    </b>Size of the source file
<b>Modify:     </b>Last modified time of the source file
<b>device:     </b>device number of the source file
<b>inode:      </b>inode number of the source file
<b>Access:     </b>Last access time of the cached entry
<b>Create:     </b>The time when this entry is stored
</pre>
</div>

See also: <a href="http://trac.lighttpd.net/xcache/wiki/PhpIni">setting php.ini for XCache</a> in the <a href="http://trac.lighttpd.net/xcache/">XCache wiki</a>
</body>
</html>

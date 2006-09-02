<h2><?php echo _T('Cache Legends'); ?></h2>
<dl>
<dt><?php echo _T('Slots'); ?>: </dt><dd>Number of hash slots. the setting from your php.ini</dd>
<dt><?php echo _T('Size'); ?>: </dt><dd>Cache Size, Size of the cache (or cache chunk), in bytes</dd>
<dt><?php echo _T('Avail'); ?>: </dt><dd>Available Memory, free memory in bytes of this cache</dd>
<dt><?php echo _T('%'); ?>: </dt><dd>Percent, A bar shows how much memory available in percent</dd>
<dt><?php echo _T('Clear'); ?>: </dt><dd>Clear Button, Press the button to clean this cache</dd>
<dt><?php echo _T('Compiling'); ?>: </dt><dd>Compiling flag, "yes" if the cache is busy compiling php script</dd>
<dt><?php echo _T('Hits'); ?>: </dt><dd>Cache Hits, hit=a var/php is loaded from this cache</dd>
<dt><?php echo _T('Misses'); ?>: </dt><dd>Cache Misses, miss=a var/php is requested but not in the cache</dd>
<dt><?php echo _T('Clogs'); ?>: </dt><dd>Compiling Clogs, clog=compiling is needed but avoided to wait(be blocked) when the cache is busy compiling already</dd>
<dt><?php echo _T('OOMs'); ?>: </dt><dd>Out Of Memory, how many times a new item should be stored but there isn't enough memory in the cache, think of increasing the xcache.size or xcache.var_size</dd>
<dt><?php echo _T('Protected'); ?>: </dt><dd>Whether <a href="http://trac.lighttpd.net/xcache/wiki/ReadonlyProtection">readonly_protection</a> is available and enable on this cache</dd>
<dt><?php echo _T('Cached'); ?>: </dt><dd>Number of entries stored in this cache</dd>
<dt><?php echo _T('Deleted'); ?>: </dt><dd>Number of entries is pending in delete list (expired but referenced)</dd>
<dt><?php echo _T('GC'); ?>: </dt><dd>Seconds count down of Garbage Collection</dd>
<dt><?php echo _T('Free Blocks'); ?>: </dt><dd>Free blocks list in the specified cache</dd>
</dl>

<h2><?php echo _T('List Legends'); ?></h2>
<dl>
<dt><?php echo _T('entry'); ?>: </dt><dd>The entry name or filename</dd>
<dt><?php echo _T('Hits'); ?>: </dt><dd>Times this entry is hit (loaded from this cache)</dd>
<dt><?php echo _T('Refcount'); ?>: </dt><dd>Reference count this entry is holded by a php request</dd>
<dt><?php echo _T('Size'); ?>: </dt><dd>Size in bytes of this entry in the cache</dd>
<dt><?php echo _T('SrcSize'); ?>: </dt><dd>Size of the source file</dd>
<dt><?php echo _T('Modify'); ?>: </dt><dd>Last modified time of the source file</dd>
<dt><?php echo _T('device'); ?>: </dt><dd>device number of the source file</dd>
<dt><?php echo _T('inode'); ?>: </dt><dd>inode number of the source file</dd>
<dt><?php echo _T('Access'); ?>: </dt><dd>Last access time of the cached entry</dd>
<dt><?php echo _T('Create'); ?>: </dt><dd>The time when this entry is stored</dd>
</dl>

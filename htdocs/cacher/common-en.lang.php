<?php

$GLOBALS['config']['show_todo_strings'] = false;

$strings = array(
		'cache.cache'
		=> 'Cache|',
		'cache.slots'
		=> 'Slots|Number of hash slots. the setting from your php.ini',
		'cache.size'
		=> 'Size|Cache Size, Size of the cache (or cache chunk), in bytes',
		'cache.avail'
		=> 'Avail|Available Memory, free memory in bytes of this cache',
		'cache.used'
		=> 'Used|Used Memory, used memory in bytes of this cache',
		'cache.blocksgraph'
		=> 'Percent Graph|Shows how much memory available in percent, and memory blocks status in graph',
		'cache.operations'
		=> 'Operations|Press the clear button to clean this cache',
		'cache.compiling'
		=> 'Comp.|Compiling flag, "yes" if the cache is busy compiling php script',
		'cache.hits'
		=> 'Hits|Cache Hits, hit=a var/php is loaded from this cache',
		'cache.hits_avg_h'
		=> 'Hits/H|Average Hits per Hour. Only last 24 hours is logged',
		'cache.hits_graph'
		=> 'Hits*24H|Hits graph of last 24 hours',
		'cache.hits_avg_s'
		=> 'Hits/S|Average Hits per Second. Only last 5 seconds is logged',
		'cache.updates'
		=> 'Updates|Cache Updates',
		'cache.clogs'
		=> 'Clogs|Compiling Clogs, clog=compiling is needed but avoided to wait(be blocked) when the cache is busy compiling already',
		'cache.ooms'
		=> 'OOMs|Out Of Memory, how many times a new item should be stored but there isn\'t enough memory in the cache, think of increasing the xcache.size or xcache.var_size',
		'cache.errors'
		=> 'Errs|Compiler errors, how many times your script is compiled but failed. You should really check what is happening if you see this value increase. (See Help for more information)',
		'cache.readonly_protected'
		=> 'Protected|Whether readonly_protection is available and enable on this cache (See help for more information)',
		'cache.cached'
		=> 'Cached|Number of entries stored in this cache',
		'cache.deleted'
		=> 'Deleted|Number of entries is pending in delete list (expired but referenced)',
		'cache.gc_timer'
		=> 'GC|Seconds count down of Garbage Collection',
		'entry.id'
		=> 'Id|',
		'entry.name'
		=> 'Entry name|The entry name or filename',
		'entry.hits'
		=> 'Hits|Times this entry is hit (loaded from this cache)',
		'entry.size'
		=> 'Size|Size in bytes of this entry in the cache',
		'entry.refcount'
		=> 'Refs|Reference count of this entry is holded by a php request',
		'entry.phprefcount'
		=> 'Shares|Count of entry sharing this php data',
		'entry.file_size'
		=> 'Src Size|Size of the source file',
		'entry.file_mtime'
		=> 'Modified|Last modified time of the source file',
		'entry.file_device'
		=> 'dev|device number of the source file',
		'entry.file_inode'
		=> 'ino|inode number of the source file',
		'entry.class_cnt'
		=> 'Cls.|Count of classes',
		'entry.function_cnt'
		=> 'Funcs|Count of functions',
		'entry.hash'
		=> 'Hash|Hash value of this entry',
		'entry.atime'
		=> 'Access|Last time when this entry is accessed',
		'entry.ctime'
		=> 'Create|The time when this entry is stored',
		'entry.delete'
		=> 'Delete|The time when this entry is deleted',
		'entry.remove'
		=> 'Remove|',
);


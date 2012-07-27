<?php

$strings = array(
		'XCache Help'
		=> 'XCache 帮助信息',
		'Help'
		=> '帮助',
		'Clear'
		=> '清除',
		'Sure to clear?'
		=> '确认要清除吗?',
		'% Free'
		=> '% 剩余',
		'% Used'
		=> '% 已用',
		'Hits'
		=> '命中',
		'Normal'
		=> '正常',
		'Compiling(%s)'
		=> '编译中(%s)',
		'Modify'
		=> '修改',
		'See also'
		=> '建议参考',
		'Legends:'
		=> '图例:',
		'Used Blocks'
		=> '已用块',
		'Free Blocks'
		=> '未用块',
		'Total'
		=> '总共',
		'Caches'
		=> '缓存区',
		'php Cached'
		=> '缓存的 php 脚本',
		'php Deleted'
		=> '待删 php 缓存',
		'var Cached'
		=> '缓存的变量',
		'var Deleted'
		=> '待删变量',
		'Statistics'
		=> '统计信息',
		'List PHP'
		=> '列出PHP',
		'List Var Data'
		=> '列变量数据',
		'XCache %s Administration'
		=> 'XCache %s 管理页面',
		'Module Info'
		=> '模块信息',
		'Remove Selected'
		=> '删除所选',
		'Editing Variable %s'
		=> '正在编辑变量 %s',
		'Set %s in config to enable'
		=> '请在配置文件中设置 %s 启用本功能',
		'cache.cache'
		=> '缓存|',
		'cache.slots'
		=> '槽|Hash 槽个数, 对应 php.ini 里的设置',
		'cache.size'
		=> '大小|共享内存区大小, 单位: 字节',
		'cache.avail'
		=> '剩余|可用内存, 对应共享内存区的剩余内存字节数',
		'cache.used'
		=> '已用|已用内存, 对应共享内存区的已用内存字节数',
		'cache.blocksgraph'
		=> '百分比图|条状显示可用内存的比例, 以及显示分配块状态',
		'cache.operations'
		=> '操作|点击按钮清除对应共享内存区的数据',
		'cache.status'
		=> '状态|状态标记. 当共享内存区正在编译 php 脚本时标记为 "编译中". 当共享内存区暂停使用时标记为 "禁用"',
		'cache.hits'
		=> '命中|共享内存命中次数, 命中=从该共享内存载入php或者变量',
		'cache.hits_avg_h'
		=> '命中/H|每小时命中次数. 只统计最后 24 小时',
		'cache.hits_graph'
		=> '24H 分布|24 小时命中分布图. 图表现是最后 24 小时的命中次数',
		'cache.hits_avg_s'
		=> '命中/S|每秒命中次数. 只统计最后 5 秒',
		'cache.updates'
		=> '更新|共享内存更新次数',
		'cache.skips'
		=> '阻塞|跳过更新次数, 跳过=XCache 自动判断阻塞的共享内存区自动跳过阻塞等待, 直接使用非共享内存方式继续处理请求',
		'cache.ooms'
		=> '内存不足|内存不足次数, 显示需要存储新数据但是共享内存区内存不足的次数. 如果出现太频繁请考虑加大配置中的 xcache.size 或者 xcache.var_size',
		'cache.errors'
		=> '错误|编译错误, 显示您的脚本被编译时出错的次数. 如果您发现这个数字不断增长, 您应该检查什么脚本产生错误. 参考 帮助 获取更多信息',
		'cache.readonly_protected'
		=> '保护|显示该 Cache 是否支持并启用 readonly_protection. 参考 帮助 获取更多信息',
		'cache.cached'
		=> '缓存|共享内存于该共享内存区的项目条数',
		'cache.deleted'
		=> '待删|共享内存区内将要删除的项目 (已经删除但是还被某些进程占用)',
		'cache.gc_timer'
		=> 'GC|垃圾回收的倒计时',
		'entry.id'
		=> 'Id|',
		'entry.name'
		=> '项目名/文件名|项目名或者文件名',
		'entry.hits'
		=> '命中|该项目被命中的次数 (从共享内存区载入)',
		'entry.size'
		=> '大小|项目在共享内存里占用字节数',
		'entry.refcount'
		=> '引用数|项目依然被其他进程占据的引用次数',
		'entry.phprefcount'
		=> '共享数|与本项目相同 PHP 代码的个数',
		'entry.file_size'
		=> '源大小|源文件大小',
		'entry.file_mtime'
		=> '修改|源文件最后修改时间',
		'entry.file_device'
		=> 'dev|源文件所在设备ID',
		'entry.file_inode'
		=> 'ino|源文件的 inode',
		'entry.class_cnt'
		=> '类|类个数',
		'entry.function_cnt'
		=> '函数|函数个数',
		'entry.hash'
		=> '哈希|该项目的哈希值',
		'entry.atime'
		=> '访问|最后访问该项目的时间',
		'entry.ctime'
		=> '创建|该项目被创建于共享内的时间',
		'entry.delete'
		=> '删除|该项目被决定删除的时间',
		'entry.remove'
		=> '删除|',
);


<h2><?php echo _T('Cache Legends'); ?></h2>
<dl>
<dt><?php echo _T('Slots'); ?>: </dt><dd>Hash 槽个数, 对应 php.ini 里的设置</dd>
<dt><?php echo _T('Size'); ?>: </dt><dd>共享内存区大小, 单位: 字节</dd>
<dt><?php echo _T('Avail'); ?>: </dt><dd>可用内存, 对应共享内存区的剩余内存字节数</dd>
<dt><?php echo _T('% Used'); ?>: </dt><dd>百分比, 条状显示可用内存的比例, 以及显示分配块状态</dd>
<dt><?php echo _T('Clear'); ?>: </dt><dd>清除按钮, 点击按钮清除对应共享内存区的数据</dd>
<dt><?php echo _T('Compiling'); ?>: </dt><dd>编译标记, 当共享内存区正在编译 php 脚本时标记为 "yes"</dd>
<dt><?php echo _T('Hits'); ?>: </dt><dd>共享内存命中次数, 命中=从该共享内存载入php或者变量</dd>
<dt><?php echo _T('Hits/H'); ?>: </dt><dd>每小时命中次数. 只统计最后 24 小时</dd>
<dt><?php echo _T('Hits 24H'); ?>: </dt><dd>24 小时命中分布图. 图表现是最后 24 小时的命中次数</dd>
<dt><?php echo _T('Hits/S'); ?>: </dt><dd>每秒命中次数. 只统计最后 5 秒</dd>
<dt><?php echo _T('Misses'); ?>: </dt><dd>共享内存错过次数, 错过=请求的php或者变量并不在该共享内存内</dd>
<dt><?php echo _T('Clogs'); ?>: </dt><dd>编译阻塞跳过, 阻塞=当需该共享内存区负责编译时, 其他进程/现成无法访问此共享内存. 跳过=XCache 自动判断阻塞的共享内存区自动跳过阻塞等待, 直接使用非共享内存方式继续处理请求</dd>
<dt><?php echo _T('OOMs'); ?>: </dt><dd>内存不足次数, 显示需要存储新数据但是共享内存区内存不足的次数. 如果出现太频繁请考虑加大配置中的 xcache.size 或者 xcache.var_size</dd>
<dt><?php echo _T('Errors'); ?>: </dt><dd>编译错误, 显示您的脚本被编译时出错的次数. 如果您发现这个数字不断增长, 您应该检查什么脚本产生错误. 参考 <a href="http://www.php.net/manual/en/ref.errorfunc.php#ini.error-log">ini.error-log</a> and <a href="http://cn2.php.net/manual/en/ref.errorfunc.php#ini.display-errors">ini.display-errors</a></dd>
<dt><?php echo _T('Protected'); ?>: </dt><dd>显示该 Cache 是否支持并启用 <a href="http://xcache.lighttpd.net/xcache/wiki/ReadonlyProtection">readonly_protection</a></dd>
<dt><?php echo _T('Cached'); ?>: </dt><dd>共享内存于该共享内存区的项目条数</dd>
<dt><?php echo _T('Deleted'); ?>: </dt><dd>共享内存区内将要删除的项目 (已经删除但是还被某些进程占用)</dd>
<dt><?php echo _T('GC'); ?>: </dt><dd>垃圾回收的倒计时</dd>
<dt><?php echo _T('Free Blocks'); ?>: </dt><dd>共享内存区内的空闲内存块</dd>
</dl>

<h2><?php echo _T('List Legends'); ?></h2>
<dl>
<dt><?php echo _T('entry'); ?>: </dt><dd>项目名或者文件名</dd>
<dt><?php echo _T('Hits'); ?>: </dt><dd>该项目被命中的次数 (从共享内存区载入)</dd>
<dt><?php echo _T('Refcount'); ?>: </dt><dd>项目依然被其他进程占据的引用次数</dd>
<dt><?php echo _T('Size'); ?>: </dt><dd>项目在共享内存里占用字节数</dd>
<dt><?php echo _T('SrcSize'); ?>: </dt><dd>源文件大小</dd>
<dt><?php echo _T('Modify'); ?>: </dt><dd>源文件最后修改时间</dd>
<dt><?php echo _T('device'); ?>: </dt><dd>源文件所在设备ID</dd>
<dt><?php echo _T('inode'); ?>: </dt><dd>源文件的inode</dd>
<dt><?php echo _T('Access'); ?>: </dt><dd>最后访问该项目的时间</dd>
<dt><?php echo _T('Create'); ?>: </dt><dd>该项目被创建于共享内的时间</dd>
</dl>

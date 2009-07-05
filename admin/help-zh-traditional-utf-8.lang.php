<h2><?php echo _T('Cache Legends'); ?></h2>
<dl>
<dt><?php echo _T('Slots'); ?>: </dt><dd>Hash 槽個數，對應 php.ini 裡的設置</dd>
<dt><?php echo _T('Size'); ?>: </dt><dd>共享記憶體區大小，單位：位元</dd>
<dt><?php echo _T('Avail'); ?>: </dt><dd>可用記憶體，對應共享記憶體區的剩餘記憶體位元數</dd>
<dt><?php echo _T('% Used'); ?>: </dt><dd>百分比，條狀顯示可用記憶體的比例</dd>
<dt><?php echo _T('Clear'); ?>: </dt><dd>清除按鈕，點擊按鈕清除對應共享記憶體區的資料</dd>
<dt><?php echo _T('Compiling'); ?>: </dt><dd>編譯標記，當共享記憶體區正在編譯 php 指令時標記為 "yes"</dd>
<dt><?php echo _T('Hits'); ?>: </dt><dd>共享記憶體命中次數，命中=從該共享記憶體載入php或者變數</dd>
<dt><?php echo _T('Misses'); ?>: </dt><dd>共享記憶體錯過次數，錯過=請求的php或者變數並不在該共享記憶體內</dd>
<dt><?php echo _T('Clogs'); ?>: </dt><dd>編譯阻塞跳過，阻塞=當需該共享記憶體區負責編譯時，其他程序/現成無法存取此共享記憶體. 跳過=XCache 自動判斷阻塞的共享記憶體區自動跳過阻塞等待，直接使用非共享記憶體方式繼續處理請求</dd>
<dt><?php echo _T('OOMs'); ?>: </dt><dd>記憶體不足次數，顯示需要儲存新資料但是共享記憶體區記憶體不足的次數. 如果出現太頻繁請考慮加大配置中的 xcache.size 或者 xcache.var_size</dd>
<dt><?php echo _T('Errors'); ?>: </dt><dd>编译错误, 显示您的脚本被编译时出错的次数. 如果您发现这个数字不断增长, 您应该检查什么脚本产生错误. 参考 <a href="http://www.php.net/manual/en/ref.errorfunc.php#ini.error-log">ini.error-log</a> and <a href="http://cn2.php.net/manual/en/ref.errorfunc.php#ini.display-errors">ini.display-errors</a></dd>
<dt><?php echo _T('Protected'); ?>: </dt><dd>顯示該 Cache 是否支援並啟用 <a href="http://xcache.lighttpd.net/xcache/wiki/ReadonlyProtection">readonly_protection</a></dd>
<dt><?php echo _T('Cached'); ?>: </dt><dd>共享記憶體於該共享記憶體區的項目個數</dd>
<dt><?php echo _T('Deleted'); ?>: </dt><dd>共享記憶體區內將要刪除的項目 (已經刪除但是還被某些程序佔用)</dd>
<dt><?php echo _T('GC'); ?>: </dt><dd>垃圾回收的倒數計時</dd>
<dt><?php echo _T('Free Blocks'); ?>: </dt><dd>共享記憶體區內的空閒記憶體區塊</dd>
</dl>

<h2><?php echo _T('List Legends'); ?></h2>
<dl>
<dt><?php echo _T('entry'); ?>: </dt><dd>項目名稱或者檔案名稱</dd>
<dt><?php echo _T('Hits'); ?>: </dt><dd>該項目被命中的次數 (從共享記憶體區載入)</dd>
<dt><?php echo _T('Refcount'); ?>: </dt><dd>項目依然被其他程序佔用的引用次數</dd>
<dt><?php echo _T('Size'); ?>: </dt><dd>項目在共享記憶體裡佔用位元數</dd>
<dt><?php echo _T('SrcSize'); ?>: </dt><dd>原始檔案大小</dd>
<dt><?php echo _T('Modify'); ?>: </dt><dd>原始檔案最後修改時間</dd>
<dt><?php echo _T('device'); ?>: </dt><dd>原始檔案所在設備ID</dd>
<dt><?php echo _T('inode'); ?>: </dt><dd>原始檔案的inode</dd>
<dt><?php echo _T('Access'); ?>: </dt><dd>最後存取該項目的時間</dd>
<dt><?php echo _T('Create'); ?>: </dt><dd>該項目被建立於共享內的時間</dd>
</dl>

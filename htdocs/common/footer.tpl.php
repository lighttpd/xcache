</div>
<div id="footer">
	<a href="http://validator.w3.org/check?uri=referer">
		<img src="http://www.w3.org/Icons/valid-xhtml10" alt="Valid XHTML 1.0 Transitional" height="31" width="88" />
	</a>
	<a href="http://feed.w3.org/check.cgi?url=http://<?php echo $_SERVER["HTTP_HOST"], $_SERVER["REQUEST_URI"]; ?>">
		<img src="http://validator.w3.org/feed/images/valid-rss-rogers.png" alt="Valid RSS" title="Validate my RSS feed" />
	</a>
	<div id="poweredBy">
		<h3>PHP <?php
	echo phpversion();
?> Powered By: XCache <?php
	echo defined('XCACHE_VERSION') ? XCACHE_VERSION : '';
?>, <?php
	echo defined('XCACHE_MODULES') ? XCACHE_MODULES : '';
?>
		</h3>
	</div>
</div>

</body>
</html>

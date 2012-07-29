<?php include("../common/header.tpl.php"); ?>
<div class="switcher"><?php echo switcher("do", $listTypes); ?></div>
<?php

include "./sub/summary.tpl.php";
$isphp = $cachelist['type'] == 'listphp';
ob_start($config['path_nicer']);

$listName = 'Cached';
$entries = $cachelist['cache_list'];
include "./sub/entrylist.tpl.php";

$listName = 'Deleted';
$entries = $cachelist['deleted_list'];
include "./sub/entrylist.tpl.php";

ob_end_flush();
unset($isphp);

?>
<?php include("../common/footer.tpl.php"); ?>

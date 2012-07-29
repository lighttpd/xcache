<?php include "../common/header.tpl.php"; ?>
<div class="switcher"><?php echo switcher("do", $doTypes); ?></div>
<?php include "./sub/summary.tpl.php"; ?>
<?php
$entryList = getEntryList();
$isphp = $entryList['type'] == 'listphp';
$typeName = $entryList['type_name'];
ob_start($config['path_nicer']);

$listName = 'Cached';
$entries = $entryList['cache_list'];
include "./sub/entrylist.tpl.php";

$listName = 'Deleted';
$entries = $entryList['deleted_list'];
include "./sub/entrylist.tpl.php";

ob_end_flush();
unset($isphp);
?>
<?php include "../common/footer.tpl.php"; ?>

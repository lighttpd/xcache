<?php

// this is default config
// DO NOT rename/delete/modify this file
// see config.example.php

// detected by browser
// $config['lang'] = 'en-us';

$config['charset'] = "UTF-8";

// translators only
$config['show_todo_strings'] = false;

// width of graph for free or usage blocks
$config['percent_graph_width'] = 120;
$config['percent_graph_type'] = 'used'; // either 'used' or 'free'

// only enable if you have password protection for admin page
// enabling this option will cause user to eval() whatever code they want
$config['enable_eval'] = false;

// this ob filter is applied for the cache list, not the whole page
$config['path_nicer'] = 'ob_filter_path_nicer_default';

?>

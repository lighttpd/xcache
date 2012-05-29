<?php

// this is default config, DO NOT modify this file
// copy this file and write your own config and name it as config.php

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

/*
function custom_ob_filter_path_nicer($list_html)
{
	$list_html = ob_filter_path_nicer_default($list_html); // this function is from common.php
	return $list_html;
}
$config['path_nicer'] = 'custom_ob_filter_path_nicer';
*/

// you can simply let xcache to do the http auth
// but if you have your home made login/permission system, you can implement the following
// {{{ home made login example
// this is an example only, it's won't work for you without your implemention.
/*
function check_admin_and_by_pass_xcache_http_auth()
{
	require("/path/to/user-login-and-permission-lib.php");
	session_start();

	if (!user_logined()) {
		if (!ask_the_user_to_login()) {
			exit;
		}
	}

	user_load_permissions();
	if (!user_is_admin()) {
		die("Permission denied");
	}

	// user is trusted after permission checks above.
	// tell XCache about it (the only way to by pass XCache http auth)
	$_SERVER["PHP_AUTH_USER"] = "moo";
	$_SERVER["PHP_AUTH_PW"] = "your-xcache-password";
	return true;
}

check_admin_and_by_pass_xcache_http_auth();
*/
// }}}

/* by pass XCache http auth
$_SERVER["PHP_AUTH_USER"] = "moo";
$_SERVER["PHP_AUTH_PW"] = "your-xcache-password";
*/

?>

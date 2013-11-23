<?php

$all_functions = get_defined_functions();
$xcache_functions = preg_grep("/^xcache_/", $all_functions['internal']);
foreach ($xcache_functions as $function) {
	ReflectionFunction::export($function);
}


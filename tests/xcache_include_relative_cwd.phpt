--TEST--
include relative to current working dir
--SKIPIF--
<?php
require("include-skipif.php");
?>
--FILE--
<?php
chdir(__DIR__);
include "./sub-a.php";
include "./sub-b.php";
?>
--EXPECTF--
%stests
%stests%ssub-a.php
%stests
%stests%ssub-b.php

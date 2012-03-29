--TEST--
include absolute path
--SKIPIF--
<?php
require("include-skipif.php");
?>
--FILE--
<?php
include __DIR__ . "/sub-a.php";
include __DIR__ . "/sub-b.php";
?>
--EXPECTF--
%stests
%stests%ssub-a.php
%stests
%stests%ssub-b.php

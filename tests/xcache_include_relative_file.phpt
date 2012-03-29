--TEST--
include relative to current file
--SKIPIF--
<?php
require("include-skipif.php");
?>
--FILE--
<?php
include "sub-a.php";
include "sub-b.php";
?>
--EXPECTF--
%stests
%stests%ssub-a.php
%stests
%stests%ssub-b.php

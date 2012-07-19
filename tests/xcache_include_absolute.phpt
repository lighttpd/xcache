--TEST--
include absolute path
--SKIPIF--
<?php
require("include-skipif.inc");
?>
--FILE--
<?php
include __DIR__ . "/sub-a.inc";
include __DIR__ . "/sub-b.inc";
?>
--EXPECTF--
%stests
%stests%ssub-a.inc
%stests
%stests%ssub-b.inc

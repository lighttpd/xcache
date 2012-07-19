--TEST--
include relative to current file
--SKIPIF--
<?php
require("include-skipif.inc");
?>
--FILE--
<?php
include "sub-a.inc";
include "sub-b.inc";
?>
--EXPECTF--
%stests
%stests%ssub-a.inc
%stests
%stests%ssub-b.inc

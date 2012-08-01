#!/usr/bin/awk -f
BEGIN {
	print "<?php";
	print "// auto generated, do not modify";
	print "$strings += array(";
}

function flushOut() {
	if (section) {
		if (section == "msgstr") {
			if (msgid == "") {
			}
			else if (msgstr == "") {
			}
			else {
				print "\t\t\""msgid"\"";
				print "\t\t=> \""msgstr"\",";
			}
		}
		else {
			print "unexpected section " section;
			exit 1;
		}
		section = null;
	}
}

/^msgid ".*"$/ {
	$0 = gensub(/^msgid "(.*)"$/, "\\1", $0);

	section = "msgid";
	msgid = $0;
	next;
}
/^msgstr ".*"$/ {
	$0 = gensub(/^msgstr "(.*)"$/, "\\1", $0);

	section = "msgstr";
	msgstr = $0;
	next;
}
/^".*"$/ {
	$0 = gensub(/^"(.*)"$/, "\\1", $0);
	if (section == "msgid") {
		msgid = msgid $0;
	}
	else {
		msgstr = msgstr $0;
	}
	next;
}
/^$/ {
	flushOut();
	next;
}
/^#/ {
	next;
}
/./ {
	print "error", $0;
	exit 1;
}
END {
	flushOut();
	print "\t\t);";
	print "";
}

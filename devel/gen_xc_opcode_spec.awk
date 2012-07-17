#! /usr/bin/awk -f
# vim:ts=4:sw=4
# process eaccelerator/opcodes.c
BEGIN {
	FS=" "
	max = 0;
	started = 0
}

/OPDEF/ {
	if (started) {
		name = "";
		if (match($0, /"([^"]+)"/, m)) { 
			name = m[1]; 
		} 
		sub(/"[^"]*"/, "");
		if (!match($0, /EXT_([^ |]+).*OP[1S]_([^ |]+).*OP2_([^ |]+).*RES_([^ |)]+).*/, array)) {
			print "error" $0
			exit
		}
		id = "";
		if (match($0, /\/\* *([0-9]+) *\*\//, comments)) {
			id = comments[1];
		}
		printf "\tOPSPEC(%10s, %10s, %10s, %10s) /* %s %-30s */\n", array[1], array[2], array[3], array[4], id, name;
		next
	}
}
/^}/ {
	print $0
	exit;
}
/^[ ]*,[ ]*$/ {
	next
}
{
	if (started) {
		print $0
		next
	}
}

/^static/ {
	started = 1;
	print "static const xc_opcode_spec_t xc_opcode_spec[] = {"
}

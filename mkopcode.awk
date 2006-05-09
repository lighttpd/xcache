#!/usr/bin/gawk -f
# vim:ts=4:sw=4
# process zend_vm_def.h or zend_compile.h
BEGIN {
	FS=" "
	max = 0;
	delete opcodes;
}

/^ZEND_VM_HANDLER\(/ {
	# regex from php5.1+/Zend/zend_vm_gen.php
	gsub(/ +/, "");
	if (!match($0, /^ZEND_VM_HANDLER\(([0-9]+),([A-Z_]+),([A-Z|]+),([A-Z|]+)\)/, array)) {
		print "error unmatch $0";
		exit;
	}
	id = 0 + array[1];
	name = array[2];
	if (max < id) {
		max = id;
	}
	opcodes[id] = name;
	next;
}

/^#define +ZEND_[A-Z_]+[\t ]+[[:digit:]]+$/ {
	id = 0 + $3;
	name = $2;
	if (max < id) {
		max = id;
	}
	opcodes[id] = name;
	next;
}

/end of block/ {
	exit;
}

END {
	mymax = 112;
	if (max < mymax) {
		for (i = max + 1; i <= mymax; i ++) {
			opcodes[i] = "UNDEF";
		}
		max = mymax;
		opcodes[110] = "ZEND_DO_FCALL_BY_FUNC";
		opcodes[111] = "ZEND_INIT_FCALL_BY_FUNC";
		opcodes[112] = "UNDEF";
	}
	printf "/* size = %d */\n", max;
	print "static const char *const xc_opcode_names[] = {";
	for (i = 0; i <= max; i ++) {
		if (i != 0) {
			print ",";
		}
		printf "/* %d */\t", i
		if (i in opcodes) {
			name = opcodes[i];
			sub(/^ZEND_/, "", name);
			printf "\"%s\"", name;
		}
		else if (i == 137) {
			printf "\"%s\"", "OP_DATA";
		}
		else {
			printf "\"UNDEF\"";
		}
	}
	print "";
	print "};";
}

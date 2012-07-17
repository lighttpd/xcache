#! /usr/bin/awk -f
# vim:ts=4:sw=4
# process zend_vm_def.h or zend_compile.h
BEGIN {
	FS=" "
	max = 0;
}

/^ZEND_VM_HANDLER\(/ {
	# regex from php5.1+/Zend/zend_vm_gen.php
	gsub(/ +/, "");
	if (!match($0, /^ZEND_VM_HANDLER\(([0-9]+),([A-Z_]+),([A-Z|]+),([A-Z|]+)\)/)) {
		print "error unmatch $0";
		exit;
	}
	# life is hard without 3rd argument of match()
	sub(/^ZEND_VM_HANDLER\(/, "");
	id = $0;
	sub(/,.*/, "", id); # chop
	id = 0 + id;
	sub(/^([0-9]+),/, "");
	sub(/,.*/, ""); # chop
	name = $0;
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
	printf "/* size = %d */\n", max + 1;
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

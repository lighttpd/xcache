#!/usr/bin/awk -f
# vim:ts=4:sw=4
BEGIN {
	brace = 0;
	buffer_len = 0;
}
/^}.*;/ {
	if (instruct) {
		sub(";", "");
		if (instruct == 1 && $2) {
			instruct = $2;
		}
		if (instruct in typedefs) {
			instruct = typedefs[instruct];
		}
		elm = "";
		elms = "";
		for (i = 0; i in buffer; i ++) {
			if (i) elm = elm " + ";
			# elm = elm "sizeof(((`" instruct "'*)NULL)->`" buffer[i] "')";
			# elms = elms " `" buffer[i] "'";
			elm = elm "sizeof(((" instruct "*)NULL)->" buffer[i] ")";

			if (elms == "") {
				elms = buffer[i];
			}
			else {
				elms = elms "," buffer[i];
			}
		}
		printf "define(`ELEMENTSOF_%s', `%s')\n", instruct, elms;
		printf "define(`COUNTOF_%s', `%s')\n", instruct, i;
		printf "define(`SIZEOF_%s', `(  %s  )')\n", instruct, elm;
		print "\n";
		for (i in buffer) {
			delete buffer[i];
		}
		buffer_len = 0;
		instruct = 0;
	}
	next;
}

/.\{/ {
	brace = brace + 1;
}
/.}/ {
	brace = brace - 1;
}

{
	if (brace == 1 && instruct) {
		sub(/.*[{}]/, "");
		gsub(/\[[^\]]+\]/, "");
		gsub(/:[0-9]+/, "");
		str = $0;
		if (match(str, /\([ ]*\*([^)]+)\)/, a)) {
			buffer[buffer_len] = a[1];
			buffer_len ++;
		}
		else {
			while (gsub(/(\([^)]*\))/, "", str)) {
			}
			while (match(str, /([^*() ]+)[ ]*[,;](.*)/, a)) {
				buffer[buffer_len] = a[1];
				buffer_len ++;
				str = a[2];
			}
		}
		next;
	}
}

/^typedef struct [^{]*;/ {
	sub(";", "");
	typedefs[$3] = $4;
	next;
}
/^typedef struct .*\{/ {
	brace = 1;
	instruct = 1;
	next;
}

/^struct .*\{/ {
	instruct = $2;
	brace = 1;
	next;
}

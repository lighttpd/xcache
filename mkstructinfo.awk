#! /usr/bin/awk -f
# vim:ts=4:sw=4
BEGIN {
	brace = 0;
	incomment = 0;
	buffer_len = 0;
}

# multiline comment handling
{
	# removes one line comment
	gsub(/\/\*(.+?)\*\//, " ");
}
/\*\// {
	if (incomment) {
		sub(/.*\*\//, "");
		incomment = 0;
	}
}
incomment {
	next;
}
/\/\*/ {
	sub(/\/\*.*/, "");
	incomment = 1;
	# fall through
}

# skip file/line mark here to be faster
/^#/ {
	next;
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
		sizeinfo = "";
		elms = "";
		for (i = 0; i in buffer; i ++) {
			if (i) {
				sizeinfo = sizeinfo " + ";
			}
			sizeinfo = sizeinfo "sizeof(((" instruct "*)NULL)->" buffer[i] ")";

			if (i == 0) {
				elms = "\"" buffer[i] "\"";
			}
			else {
				elms = elms "," "\"" buffer[i] "\"";
			}
		}
		printf "define(`ELEMENTSOF_%s', `%s')\n", instruct, elms;
		printf "define(`COUNTOF_%s', `%s')\n", instruct, i;
		printf "define(`SIZEOF_%s', `(  %s  )')\n", instruct, sizeinfo;
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
		gsub(/(^[\t ]+|[\t ]+$)/, ""); # trim whitespaces
		sub(/.*[{}]/, "");
		gsub(/\[[^\]]+\]/, ""); # ignore [...]
		gsub(/:[0-9]+/, ""); # ignore struct bit
		if (match($0, /^[^(]*\([ ]*\*([^)]+)\)/)) {
			sub(/ +/, "")
			sub(/^[^(]*\(\*/, "");
			sub(/\).*/, "");
			# function pointer
			buffer[buffer_len] = $0;
			buffer_len ++;
		}
		else {
			# process normal variables

			# ignore any ()s
			while (gsub(/(\([^)]*\))/, "")) {
			}
			if (match($0, /[()]/)) {
				next;
			}
			# unsigned int *a,  b; int c;
			gsub(/[*]/, " ");
			# unsigned int a,  b; int c;
			gsub(/ +/, " ");
			# unsigned int a, b; int c;
			gsub(/ *[,;]/, ";");
			# unsigned int a; b; int c;
			if (!match($0, /;/)) {
				next;
			}
			# print "=DEBUG=" $0 "==";
			split($0, chunks, ";");
			# [unsigned int a, b, c]

			for (i = 1; i in chunks; i ++) {
				if (chunks[i] == "") {
					delete chunks[i];
					continue;
				}
				split(chunks[i], pieces, " ");
				# [unsigned, int, a]
				# [b]
				# [c]

				last_piece = "";
				for (j = 1; j in pieces; j ++) {
					last_piece = pieces[j];
					delete pieces[j];
				}
				if (last_piece == "") {
					# print "=ERROR=" chunks[i] "==";
					delete chunks[i];
					continue;
				}
				# a
				# b
				# c

				buffer[buffer_len] = last_piece;
				buffer_len ++;
				delete chunks[i]
			}
			last_piece = "";
		}
		next;
	}
}

/^typedef struct [^{]*;/ {
	sub(";", "");
	typedefs[$3] = $4;
	next;
}
/^typedef struct .*\{[^}]*$/ {
	brace = 1;
	instruct = 1;
	next;
}

/^struct .*\{/ {
	instruct = $2;
	brace = 1;
	next;
}

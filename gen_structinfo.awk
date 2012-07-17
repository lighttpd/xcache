#! /usr/bin/awk -f
# vim:ts=4:sw=4
BEGIN {
	brace = 0;
	incomment = 0;
	buffer_len = 0;
}
function printstruct(structname) {
	printf "define(`ELEMENTSOF_%s', `%s')\n", structname, ELEMENTSOF[structname];
	printf "define(`COUNTOF_%s', `%s')\n", structname, COUNTOF[structname];
	printf "define(`SIZEOF_%s', `(  %s  )')\n", structname, SIZEOF[structname];
}
function countBrace(text,  len, i, char, braceCount) {
	len = length(text);
	braceCount = 0;
	for (i = 1; i <= len; ++i) {
		char = substr(text, i, 1);
		if (char == "{") {
			braceCount = braceCount + 1;
		}
		else if (char == "}") {
			braceCount = braceCount - 1;
		}
	}
	return braceCount;
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
		structname = instruct;
		if (structname == 1 && $2) {
			structname = $2;
		}
		if (structname in typedefs) {
			structname = typedefs[structname];
		}
		sizeinfo = "";
		elms = "";
		for (i = 0; i in buffer; i ++) {
			if (i) {
				sizeinfo = sizeinfo " + ";
			}
			sizeinfo = sizeinfo "sizeof(((" structname "*)NULL)->" buffer[i] ")";

			if (i == 0) {
				elms = "\"" buffer[i] "\"";
			}
			else {
				elms = elms "," "\"" buffer[i] "\"";
			}
		}
		ELEMENTSOF[structname] = elms;
		COUNTOF[structname]    = i;
		SIZEOF[structname]     = sizeinfo;
		printstruct(structname);
		print "\n";
		for (i in buffer) {
			delete buffer[i];
		}
		buffer_len = 0;
		instruct = 0;
	}
	next;
}

/.[{}]/ {
	brace += countBrace($0);
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
	typename=$3;
	newtypename=$4;
	typedefs[typename] = newtypename;
	if (ELEMENTSOF[typename]) {
		ELEMENTSOF[newtypename] = ELEMENTSOF[typename];
		COUNTOF[newtypename]    = COUNTOF[typename];
		sub(/.*/, SIZEOF[typename]);
		gsub(typename, newtypename);
		SIZEOF[newtypename]     = $0;
		printstruct(newtypename);
	}
	next;
}
/^typedef struct .*\{[^}]*$/ {
	brace = countBrace($0);
	if (brace > 0) {
		instruct = 1;
	}
	else {
		brace = 0;
		instruct = 0;
	}

	for (i in buffer) {
		delete buffer[i];
	}
	next;
}

/^struct .*\{.*/ {
	brace = countBrace($0);
	if (brace > 0) {
		instruct = $2;
	}
	else {
		brace = 0;
		instruct = 0;
	}

	for (i in buffer) {
		delete buffer[i];
	}
	next;
}

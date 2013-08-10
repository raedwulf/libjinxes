#!/usr/bin/env awk -f

BEGIN {
	template = "terminfo.def.h"
	infocmp = "infocmp -L -1 "
        termcaps = "cpp -DTERMINFO_RAW_NAMES -DTERMINFO_STRING " template

	while ((getline < template) > 0)
		print $0

	i = 1
	j = 1
	while ((termcaps | getline) > 0)
		if ($1 != "" && $1 != "#") {
			caps[tolower($1)] = i
			caps[i++] = tolower($1)
		}
	num = i

	print "\n#ifdef TERMINFO_ESCAPE_CODES\n"
	print "\ntypedef struct {"
	print "\tshort location;"
	print "\tshort esc;"
	print "} terminal_variant;"
	FS="="
	RS=" "
	esc = 1
	esc8 = 1
	esc12 = 1
}

{
	term = $0
	gsub(/\n/,"",term)
	term_ = term
	gsub(/-/,"_",term_)
	sub(/^9/,"_9",term_)
	gsub(/\+/,"x",term_)
	gsub(/\./,"_",term_)
	RS="\n"
	while (((infocmp term) | getline) > 0) {
		gsub(/^[ \t]+|[, \t]+$/, "")
		#gsub(/\\/, "\\\\")
		#gsub(/\\\\E/, "\\033")
		gsub(/\\E/, "\\033")
		gsub(/"/, "\\\"")
		gsub(/\^~/,  "\\000")
		gsub(/\^2/,  "\\000")
		gsub(/\^A/,  "\\001")
		gsub(/\^B/,  "\\002")
		gsub(/\^C/,  "\\003")
		gsub(/\^D/,  "\\004")
		gsub(/\^E/,  "\\005")
		gsub(/\^F/,  "\\006")
		gsub(/\^G/,  "\\007")
		gsub(/\^H/,  "\\010")
		gsub(/\^I/,  "\\011")
		gsub(/\^J/,  "\\012")
		gsub(/\^K/,  "\\013")
		gsub(/\^L/,  "\\014")
		gsub(/\^M/,  "\\015")
		gsub(/\^N/,  "\\016")
		gsub(/\^O/,  "\\017")
		gsub(/\^P/,  "\\020")
		gsub(/\^Q/,  "\\021")
		gsub(/\^R/,  "\\022")
		gsub(/\^S/,  "\\023")
		gsub(/\^T/,  "\\024")
		gsub(/\^U/,  "\\025")
		gsub(/\^V/,  "\\026")
		gsub(/\^W/,  "\\027")
		gsub(/\^X/,  "\\030")
		gsub(/\^Y/,  "\\031")
		gsub(/\^Z/,  "\\032")
		gsub(/\^\[/, "\\033")
		gsub(/\^3/,  "\\033")
		gsub(/\^4/,  "\\034")
		gsub(/\^\\/, "\\034")
		gsub(/\^5/,  "\\035")
		gsub(/\^\]/, "\\035")
		gsub(/\^6/,  "\\036")
		gsub(/\^7/,  "\\037")
		gsub(/\^\//, "\\037")
		gsub(/\^_/,  "\\037")
		gsub(/\^8/,  "\\177")
		gsub(/\\$/,  "\\\\")
		if (caps[$1]) {
			if (esci[$2])
				capsv[j,$1] = esci[$2] - 1
			else {
				if (length($2) <= 8) {
					tbl = 16384
					capsv[j,$1] = esc8 + tbl - 1
					esci[$2] = esc8 + tbl
					escp8[esc8] = $2
					esc8++
				} else if (length($2) <= 12) {
					tbl = 32768
					capsv[j,$1] = esc12 + tbl - 1
					esci[$2] = esc12 + tbl
					escp12[esc12] = $2
					esc12++
				} else {
					capsv[j,$1] = esc - 1
					esci[$2] = esc
					escp[esc] = $2
					esc++
				}
			}
		}
	}
	for (i = 1; i < num; i++) {
		if (!capsv[j,caps[i]]) capsv[j,caps[i]] = -1
	}
	RS=" "
	close (infocmp)

	capstr = ""
	for (i = 1; i < num; i++)
		capstr = capstr capsv[j,caps[i]] (i == num - 1 ? "" : ",")

	largest_sameness = -1
	largest_j = -1
	for (i = 1; i < j; i++) {
		sameness[i] = 1
		for (k = 1; k < num; k++) {
			if (capsv[j,caps[k]] == capsv[i,caps[k]])
				sameness[i]++;
		}
		if (sameness[i] > largest_sameness) {
			largest_sameness = sameness[i]
			largest_j = i
		}
	}

	if (largest_sameness > (num / 2)) {
		if (capstrdic[capstr]) {
			terminals = terminals "{\"" term "\","capstrdic[capstr]",-1} /* "j - 1" */,\n"
		} else if (largest_sameness == num) {
			terminals = terminals "{\"" term "\","varstrdic[largest_j]"," parent[largest_j] "} /* "j - 1" */,\n"
		} else {
			parent[j] = largest_j - 1
			varstrdic[j] = term_"_var"
			varstr = ""
			for (i = 1; i < num; i++) {
				if (capsv[j,caps[i]] != capsv[largest_j,caps[i]])
					varstr = varstr "{" (i - 1) "," capsv[j,caps[i]] "},"
			}
			printf "static const terminal_variant "term_"_var[] = {"
			printf "%s{-1,-1}", varstr
			print "};"
			terminals = terminals "{\"" term "\","term_"_var," (largest_j - 1) "} /* "j - 1" */,\n"
		}
	} else {
		#if (!capstrdic[capstr]) {
		capstrdic[capstr] = term_"_esc"
		printf "static const short "term_"_esc[] = {"
		printf "%s", capstr
		print "};"
		#}
		terminals = terminals "{\"" term "\","capstrdic[capstr]", -1} /* "j - 1" */,\n"
	}

	j++
}

END {
	print "\nstatic const char terminfo_short8_esctable[][8] = {"
	for (i = 1; i < esc8; i++)
		print "\""escp8[i]"\"" (i < esc8 - 1 ? "," : "") " /* " esci[escp8[i]] - 1 " */"
	print "};"
	print "\nstatic const char terminfo_short12_esctable[][12] = {"
	for (i = 1; i < esc12; i++)
		print "\""escp12[i]"\"" (i < esc12 - 1 ? "," : "") " /* " esci[escp12[i]] - 1 " */"
	print "};"
	print "\nstatic const char *terminfo_long_esctable[] = {"
	for (i = 1; i < esc; i++)
		print "\""escp[i]"\"" (i < esc - 1 ? "," : "") " /* " esci[escp[i]] - 1 " */"
	print "};"
	print "\ntypedef struct {"
	print "\tconst char *name;"
	print "\tconst void *esc;"
	print "\tshort parent;"
	print "} terminal_map;\n"
	printf "static terminal_map terminals[] = {\n%s{NULL,NULL,-1}\n};\n", terminals
	print "\n#endif"
}

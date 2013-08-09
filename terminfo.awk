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
	print "\tconst char *esc;"
	print "} terminal_variant;"
	FS="="
	RS=" "
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
		gsub(/\\/, "\\\\")
		gsub(/\\\\E/, "\\033")
		gsub(/"/, "\\\"")
		if (caps[$1]) capsv[j,$1] = $2
	}
	RS=" "
	close (infocmp)

	capstr = ""
	for (i = 1; i < num; i++)
		capstr = capstr "\"" capsv[j,caps[i]] "\"" (i == num - 1 ? "" : ",")

	largest_sameness = -1
	largest_j = -1
	for (i = 1; i < j - 1; i++) {
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
			terminals = terminals "{\"" term "\","capstrdic[capstr]",-1,NULL},"
		} else if (largest_sameness == num) {
			terminals = terminals "{\"" term "\",NULL," parent[largest_j] ","varstrdic[largest_j]"},"
		} else {
			parent[j] = largest_j - 1
			varstrdic[j] = term_"_var"
			varstr = ""
			for (i = 1; i < num; i++) {
				if (capsv[j,caps[i]] != capsv[largest_j,caps[i]])
					varstr = varstr "{" (i - 1) ",\"" capsv[j,caps[i]] "\"},"
			}
			printf "static const terminal_variant "term_"_var[] = {"
			printf "%s", varstr
			print "};"
			terminals = terminals "{\"" term "\",NULL," (largest_j - 1) ","term_"_var},"
		}
	} else {
		#if (!capstrdic[capstr]) {
		capstrdic[capstr] = term_"_esc"
		printf "static const char *"term_"_esc[] = {"
		printf "%s", capstr
		print "};"
		#}
		terminals = terminals "{\"" term "\","capstrdic[capstr]", -1, NULL},"
	}

	j++
}

END {
	print "\ntypedef struct {"
	print "\tconst char *name;"
	print "\tconst char **esc;"
	print "\tshort parent_terminal;"
	print "\tconst terminal_variant *variant;"
	print "} terminal_map;\n"
	printf "static terminal_map terminals[] = {%s{NULL,NULL,-1,NULL}};\n", terminals
	print "\n#endif"
}

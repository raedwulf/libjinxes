/* See LICENSE file for copyright and license details. */

#include "jinxes.h"
#include <stdio.h>
#include <string.h>

#define TEST_LOG "jinxestest.log"
#define TEST_ASSERT(f,a,c) ftest(#a " " #c, a, a c, f);

void ftest(const char *s, int value, int pass, FILE *f)
{
	fprintf(f, "%s...%d\t%s\n", s, value, pass ? "PASS" : "FAIL");
}

int main(int argc, char **argv)
{
	FILE *f = fopen(TEST_LOG, "w+");
	int err = JX_SUCCESS;
	if ((err = jx_initialise())) {
		fprintf(stderr, "%s: %s\n", argv[0], jx_error(err));
		return 1;
	}
	TEST_ASSERT(f, jx_columns(), > 0);
	TEST_ASSERT(f, jx_lines(), > 0);
	jx_terminate();
	fclose(f);
	return 0;
}

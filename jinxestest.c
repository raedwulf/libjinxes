/* See LICENSE file for copyright and license details. */

#include "jinxes.h"
#include <stdio.h>

int main(int argc, char **argv)
{
	int err = JX_SUCCESS;
	if ((err = jx_init())) {
		fprintf(stderr, "%s: %s\n", argv[0], jx_error(err));
		return 1;
	}
	jx_end();
	return 0;
}

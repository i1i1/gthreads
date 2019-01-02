#include <stdio.h>

#include "gthreads.h"


void
pingpong(void *total_p)
{
	int who;
	int total;

	total = (size_t)total_p;

	for (;;) {
		int i;

		i = (size_t)gthreads_recieve(&who);

		printf("|% 5d|% 5d|% 5d|\n", who, gthreads_getid(), i);

		if (i == total) {
			printf("+-----------------+\n");
			gthreads_exit();
			return;
		}

		gthreads_send(who, (void *)(size_t)++i);

		if (i == total) {
			gthreads_exit();
			return;
		}
	}
}

int
main(void)
{
	int who;

	who = gthreads_spawn(pingpong, (void *)11);

	printf("send 0 from %x to %x\n", gthreads_getid(), who);

	printf("+-----+-----+-----+\n");
	printf("|   to| from|  val|\n");
	printf("|-----+-----+-----|\n");

	gthreads_send(who, 0);
	pingpong((void *)11);
}


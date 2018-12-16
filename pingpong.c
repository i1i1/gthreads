#include <stdio.h>

#include "gthreads.h"


void
pingpong(void)
{
	int who;
	int total;

	total = 10;

	for (;;) {
		int i;

		i = (size_t)gthreads_recieve(&who);

		printf("%x got %d from %x\n", gthreads_getid(), i, who);

		if (i == total)
			return;

		gthreads_send(who, (void *)(size_t)++i);

		if (i == total)
			return;
	}
}

int
main(void)
{
	int who;

	who = gthreads_spawn(pingpong);

	printf("send 0 from %x to %x\n", gthreads_getid(), who);

	gthreads_send(who, 0);
	pingpong();
}


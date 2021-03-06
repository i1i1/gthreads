#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include "gthreads.h"


enum {
	OK		= 0,
	ERR		= 1,
	DONE	= 2,
};

void
check_mod(void *mod_p)
{
	size_t mod, num;
	int n, next_thread;

	next_thread = 0;
	mod = (size_t)mod_p;

	for (;;) {
		num = (size_t)gthreads_recieve(&n);

		if (num == DONE) {
			if (next_thread)
				gthreads_send(next_thread, (void *)DONE);
			gthreads_exit();
		}

		if (num % mod == 0) {
			gthreads_send(n, (void *)ERR);
			continue;
		}
		if (next_thread) {
			gthreads_send(next_thread, (void *)num);
			gthreads_send(n, gthreads_recieve(NULL));
			continue;
		}

		next_thread = gthreads_spawn(check_mod, (void *)num);
		gthreads_send(n, (void *)OK);
	}
}

int
main(int argc, char **argv)
{
	size_t i, n, thread;

	if (argc < 2)
		return 1;

	i = 2;
	n = atoll(argv[1]);
	thread = gthreads_spawn(check_mod, (void *)i);

	printf("%d\n", 2);

	while (++i <= n){
		int ret;

		gthreads_send(thread, (void *)i);
		ret = (size_t)gthreads_recieve(NULL);

		if (ret == OK)
			printf("%ld\n", i);
	}

	gthreads_send(thread, (void *)DONE);
	gthreads_exit();
}


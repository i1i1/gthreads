#include <stdio.h>
#include <stdlib.h>

#include "gthreads.h"


static void
yield_thread(void)
{
	int i;

	printf("Hello, I am environment %d.\n", gthreads_getid());

	for (i = 0; i < 5; i++) {
		printf("In cycle!\n");
		if (gthreads_switch())
			printf("Oooops! failed to switch!\n");

		printf("Back in environment %d, iteration %d.\n",
		       gthreads_getid(), i);
	}

	printf("All done in environment %d.\n", gthreads_getid());
	gthreads_exit();
}

int
main(int argc, char **argv)
{
	int i, n;

	if (argc < 2)
		return 1;

	n = atoi(argv[1]);

	for (i = 0; i < n; i++) {
		if (gthreads_spawn(yield_thread) == -1)
			break;
	}

	gthreads_exit();
}


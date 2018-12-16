#include <stdio.h>

#include "gthreads.h"


static void
yield_thread(void)
{
	int i;

	printf("Hello, I am environment %d.\n", gthreads_getid());

	for (i = 0; i < 5; i++) {
		if (gthreads_switch())
			printf("Oooops! failed to switch!\n");

		printf("Back in environment %d, iteration %d.\n",
		       gthreads_getid(), i);
	}

	printf("All done in environment %d.\n", gthreads_getid());
	gthreads_exit();
}

int
main()
{
	int i, n;

	scanf("%d", &n);

	for (i = 0; i < n; i++) {
		if (gthreads_spawn(yield_thread) == -1)
			break;
	}

	gthreads_exit();
}


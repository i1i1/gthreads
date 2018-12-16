#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include "gthreads.h"

#define VECTOR_MAXNMEMB_START	8
#define VECTOR_MAXNMEMB_GROWTH	1.4

#define VECTOR_REALLOC(ptr, size)	realloc(ptr, size)
#define VECTOR_FREE(ptr)			VECTOR_REALLOC(ptr, 0)
#define VECTOR_MALLOC(size)			VECTOR_REALLOC(NULL, size)

#define vector_nmemb(a)		((a) ? (size_t)_vector_raw(a)[0] : 0)

#define _vector_raw(a)		((size_t *)(a) - 2)
#define _vector_nmemb(a)	(_vector_raw(a)[0])
#define _vector_maxnmemb(a)	(_vector_raw(a)[1])

#define vector_push(a, val)	(((a) && !_vector_needgrow(a, 1)) ? \
								(_vector_hardpush(a, val), 0) : \
									(_vector_grow((void *)&(a), sizeof(*(a))) ? 1 : \
										(_vector_hardpush(a, val), 0)))
#define vector_pop(a)		(assert(vector_nmemb(a)), (a)[--_vector_nmemb(a)])
#define vector_free(a)		((a) ? VECTOR_FREE(_vector_raw(a)) : 0)

#define _vector_hardpush(a, val) ((a)[_vector_nmemb(a)++] = val)
#define _vector_needgrow(a, n)	((_vector_nmemb(a) + n) > _vector_maxnmemb(a))

int
_vector_grow(void **pp, int sz)
{
	void *p = *pp;
	size_t nmax;

	if (!p) {
		p = VECTOR_MALLOC(sizeof(size_t) * 2 + sz * VECTOR_MAXNMEMB_START);

		if (!p)
			return 1;

		p = (size_t *)p + 2;
		_vector_nmemb(p) = 0;
		_vector_maxnmemb(p) = VECTOR_MAXNMEMB_START;

		*pp = p;

		return 0;
	}

	nmax = (float)(_vector_maxnmemb(p) * VECTOR_MAXNMEMB_GROWTH);
	p = VECTOR_REALLOC(_vector_raw(p), sizeof(size_t) * 2 + sz * nmax);

	if (!p)
		return 1;

	p = (size_t *)p + 2;
	_vector_maxnmemb(p) = nmax;

	*pp = p;

	return 0;
}

enum {
	DONE = 0,
	MOD = 1,
};

void
check_mod(void)
{
	size_t mod, num;
	int n;

	mod = (size_t)gthreads_recieve(&n);

	while ((num = (size_t)gthreads_recieve(&n)) != DONE) {
		if (num == MOD)
			gthreads_send(n, (void *)mod);
		else
			gthreads_send(n, (void *)(num % mod));
	}

	gthreads_exit();
}

int
main(int argc, char **argv)
{
	size_t i, j, n, mod;
	int tmp;
	int *threads;
	size_t *sieve;

	if (argc < 2)
		return 1;

	sieve = NULL;
	threads = NULL;
	n = atoll(argv[1]);

	i = 1;

	while (++i <= n){
		for (j = 0, mod = 1; j < vector_nmemb(threads); j++) {
			gthreads_send(threads[j], (void *)i);

			if ((mod = (size_t)gthreads_recieve(NULL)) == 0)
				break;
		}

		if (mod == 0)
			continue;

		if ((tmp = gthreads_spawn(check_mod)) == -1)
			goto finish;
		if (vector_push(threads, tmp))
			goto finish;

		gthreads_send(tmp, (void *)i);
	}

finish:
	for (i = 0; i < vector_nmemb(threads); i++) {
		gthreads_send(threads[i], (void *)MOD);
		vector_push(sieve, (size_t)gthreads_recieve(NULL));
		gthreads_send(threads[i], (void *)DONE);
	}

	for (i = 0; i < vector_nmemb(sieve); i++)
		printf("%ld\n", sieve[i]);

	vector_free(threads);
	vector_free(sieve);

	gthreads_exit();
}


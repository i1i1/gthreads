#ifndef _GTHREADS_H_
#define _GTHREADS_H_

#define GTHREADS_MAXTHREADS	100


typedef void gthreads_entry(void);


/*
 * Returns -1 if fails and id of new thread on success.
 */
int gthreads_spawn(gthreads_entry *entry);
void gthreads_destroy(int thrd);

/*
 * Sends `val` to thread with id `dst`.
 */
int gthreads_send(int dst, void *val);
/*
 * Receives value from other thread.
 * Sets `thrd` with id of sender and returns send value.
 */
void *gthreads_recieve(int *thrd);

/*
 * Returns id of current environment.
 */
int gthreads_getid(void);

/*
 * Switches manually to other available thread,
 * or returns 1 if fails.
 */
int gthreads_switch(void);

/*
 * Exits from current thread.
 * Should be also invoked in main thread in order to exit!
 */
void gthreads_exit(void);

#endif /* _GTHREADS_H_ */


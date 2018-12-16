#ifndef _GTHREADS_H_
#define _GTHREADS_H_


typedef void gthreads_entry(void);


int gthreads_spawn(gthreads_entry *entry);
void gthreads_destroy(int thrd);

int gthreads_send(int dst, void *val);
void *gthreads_recieve(int *thrd);

int gthreads_getid(void);

void gthreads_exit(void);


#endif /* _GTHREADS_H_ */

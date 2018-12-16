#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ucontext.h>
#include <time.h>

#include <assert.h>

#include <stdio.h>

#include "gthreads.h"


#define GTHREADS_STACKSIZE	(4 * 4096)

#define SIG_TIMER			SIGRTMIN


struct thread_info {
	struct thread_info *prev;
	struct thread_info *next;
	ucontext_t ctx;
	int id;

	void *msg;
	int sender;
};


static struct thread_info *head;
static struct thread_info *cur_thread;
static int total_threads = 0;

static timer_t timer;
static ucontext_t exiter;
static int nextid;

/* In order to not `free` it */
static void *native_stack;


int
gthreads_switch(void)
{
	volatile int here;

	here = 0;

	if (getcontext(&cur_thread->ctx))
		return 1;

	if (here++ == 0) {
		cur_thread = cur_thread->next;

		if (setcontext(&cur_thread->ctx)) {
			/* Restore previous thread data */
			cur_thread = cur_thread->prev;
			return 1;
		}
	}

	return 0;
}

static void
tick(int signo, siginfo_t *siginfo, void *cont)
{
	(void) signo;
	(void) siginfo;
	(void) cont;

	gthreads_switch();
}

static int
stack_init(stack_t *s)
{
	if (!(s->ss_sp = malloc(GTHREADS_STACKSIZE)))
		return 1;

	s->ss_size = GTHREADS_STACKSIZE;
	return 0;
}

static void
stack_free(stack_t *s)
{
	free(s->ss_sp);
	s->ss_sp = NULL;
	s->ss_size = 0;
}

static int
gthreads_init_timer(void)
{
	struct sigaction sigact;
	struct sigevent sigev;

	sigact.sa_flags = SA_SIGINFO;
	sigact.sa_sigaction = tick;

	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = SIG_TIMER;

	if (sigemptyset(&sigact.sa_mask))
		return 1;

	if (sigaction(SIG_TIMER, &sigact, NULL))
		return 1;

	if (timer_create(CLOCK_PROCESS_CPUTIME_ID, &sigev, &timer))
		return 1;

	return 0;
}

static int
gthreads_update_timer(void)
{
	struct itimerspec tm;

	if (total_threads == 1) {
		tm.it_value.tv_sec  = 1;
		tm.it_value.tv_nsec = 0;
	} else {
		tm.it_value.tv_sec  = 0;
		tm.it_value.tv_nsec = 1000000000/total_threads;
	}

	tm.it_interval = tm.it_value;

	return timer_settime(timer, 0, &tm, NULL);
}

static int
gthreads_init_exiter(void)
{
	if (getcontext(&exiter))
		return 1;
	if (stack_init(&exiter.uc_stack))
		return 1;
	makecontext(&exiter, gthreads_exit, 0);
	return 0;
}

static int
gthreads_init(void)
{
	if (gthreads_init_timer())
		return 1;

	if (gthreads_init_exiter())
		return 1;

	if (!(head = malloc(sizeof(*head))))
		return 1;

	if (getcontext(&head->ctx))
		return 1;

	native_stack = head->ctx.uc_stack.ss_sp;

	head->sender = -1;
	head->id = 0;
	head->next = head->prev = head;
	head->ctx.uc_link = &exiter;

	total_threads = 1;
	cur_thread = head;

	nextid = 1;

	return 0;
}

int
gthreads_spawn(gthreads_entry *entry)
{
	struct thread_info *nt;

	assert(entry);

	if (total_threads == 0 && gthreads_init())
		return -1;

	if (!(nt = malloc(sizeof(*nt))))
		return -1;

	if (getcontext(&nt->ctx))
		return -1;

	if (stack_init(&nt->ctx.uc_stack))
		return -1;

	makecontext(&nt->ctx, entry, 0);

	nt->prev = cur_thread;
	nt->next = cur_thread->next;
	cur_thread->next = nt;

	nt->id = nextid++;
	nt->sender = -1;
	nt->ctx.uc_link = &exiter;

	total_threads++;
	gthreads_update_timer();

	return nt->id;
}

void
gthreads_destroy(int thrd)
{
	struct thread_info *tp;

	if (!total_threads)
		return;

	tp = head;

	if (head->id == thrd)
		head = head->next;

	do {
		if (tp->id != thrd)
			continue;

		if (tp->ctx.uc_stack.ss_sp != native_stack)
			stack_free(&tp->ctx.uc_stack);

		tp->next->prev = tp->prev;
		tp->prev->next = tp->next;

		total_threads--;
		gthreads_update_timer();

		if (tp == cur_thread) {
			cur_thread = cur_thread->next;
			free(tp);
			setcontext(&cur_thread->ctx);
			return;
		}

		free(tp);
		return;

	} while ((tp = tp->next) != head);
}

int
gthreads_send(int dst, void *val)
{
	struct thread_info *tp;

	tp = head;

	do {
		if (tp->id == dst)
			break;
	} while ((tp = tp->next) != head);

	if (tp->id != dst)
		return 1;

	tp->msg = val;
	tp->sender = cur_thread->id;

	return 0;
}

void *
gthreads_recieve(int *thrd)
{
	assert(thrd);

	/* Waiting for message */
	while (cur_thread->sender == -1)
		gthreads_switch();

	*thrd = cur_thread->sender;
	cur_thread->sender = -1;

	return cur_thread->msg;
}

int
gthreads_getid()
{
	return cur_thread->id;
}

void
gthreads_exit(void)
{
	gthreads_destroy(cur_thread->id);
}


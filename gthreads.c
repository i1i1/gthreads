#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ucontext.h>
#include <time.h>

#include <assert.h>

#include "gthreads.h"


#define GTHREADS_STACKSIZE	1024

#define SIG_TIMER			SIGRTMIN

typedef char gstack_t[GTHREADS_STACKSIZE];


struct thread_info {
	struct thread_info *prev;
	struct thread_info *next;
	ucontext_t ctx;
	gstack_t st;
	int id;

	void *msg;
	int sender;
};


static struct thread_info *head;
static struct thread_info *cur_thread;
static int total_threads = 0;

static timer_t timer;
static int nextid;

static struct itimerspec unarmtm, mintm, tm;


static int gthreads_arm_timer(void);
static int gthreads_disarm_timer(void);


int
gthreads_switch(void)
{
	static volatile int here;

	if (total_threads < 2)
		return 0;

	here = 0;
	gthreads_disarm_timer();

	if (getcontext(&cur_thread->ctx))
		return 1;

	if (here > 0)
		return 0;

	here++;
	cur_thread = cur_thread->next;
	gthreads_arm_timer();

	if (setcontext(&cur_thread->ctx)) {
		/* Restore previous thread data */
		cur_thread = cur_thread->prev;
		return 1;
	}

	/* Will never reach */
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

static void
stack_init(stack_t *s, void *st)
{
	s->ss_sp = st;
	s->ss_size = GTHREADS_STACKSIZE;
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
	sigev.sigev_value.sival_ptr = NULL;

	unarmtm.it_interval.tv_sec = 0;
	unarmtm.it_interval.tv_nsec = 0;
	unarmtm.it_value.tv_sec = 0;
	unarmtm.it_value.tv_nsec = 0;

	mintm.it_interval.tv_sec = 0;
	mintm.it_interval.tv_nsec = 0;
	mintm.it_value.tv_sec = 0;
	mintm.it_value.tv_nsec = GTHREADS_MINTIMER;

	tm.it_interval.tv_sec = 0;
	tm.it_interval.tv_nsec = 0;
	tm.it_value.tv_sec  = 0;

	if (sigemptyset(&sigact.sa_mask))
		return 1;

	if (sigaction(SIG_TIMER, &sigact, NULL))
		return 1;

	if (timer_create(CLOCK_PROCESS_CPUTIME_ID, &sigev, &timer))
		return 1;

	return 0;
}

static int
gthreads_disarm_timer(void)
{
	return timer_settime(timer, 0, &unarmtm, NULL);
}

static int
gthreads_arm_timer(void)
{
	if (total_threads == 1)
		return gthreads_disarm_timer();

	if (total_threads >= GTHREADS_MAXTHREADS)
		return timer_settime(timer, 0, &mintm, NULL);

	tm.it_value.tv_nsec = 1000000000/total_threads;

	return timer_settime(timer, 0, &tm, NULL);
}

static int
gthreads_init(void)
{
	if (gthreads_init_timer())
		return 1;

	if (!(head = malloc(sizeof(struct thread_info))))
		return 1;

	head->sender = -1;
	head->id = 0;
	head->next = head->prev = head;

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

	gthreads_disarm_timer();

	if (!(nt = malloc(sizeof(struct thread_info))))
		return -1;

	if (getcontext(&nt->ctx))
		return -1;

	stack_init(&nt->ctx.uc_stack, &nt->st);
	makecontext(&nt->ctx, entry, 0);

	nt->id = nextid++;
	nt->sender = -1;

	nt->prev = cur_thread;
	nt->next = cur_thread->next;
	cur_thread->next = nt;
	nt->next->prev = nt;

	total_threads++;
	gthreads_arm_timer();

	return nt->id;
}

void
gthreads_destroy(int thrd)
{
	struct thread_info *tp;

	gthreads_disarm_timer();
	tp = head;

	if (head->id == thrd)
		head = head->next;

	do {
		if (tp->id != thrd)
			continue;

		if (total_threads == 1) {
			free(tp);
			timer_delete(timer);
			exit(0);
		}

		tp->next->prev = tp->prev;
		tp->prev->next = tp->next;
		total_threads--;

		if (tp == cur_thread) {
			cur_thread = cur_thread->next;
			free(tp);
			gthreads_arm_timer();
			setcontext(&cur_thread->ctx);
			return;
		}

		free(tp);
		gthreads_arm_timer();
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

	/* Waiting for receiver to get all the messages */
	while (tp->sender != -1)
		gthreads_switch();

	tp->sender = cur_thread->id;
	tp->msg = val;

	/* Waiting for receiver to recieve this message */
	while (tp->sender == cur_thread->id)
		gthreads_switch();

	return 0;
}

void *
gthreads_recieve(int *thrd)
{
	void *res;

	/* Waiting for message */
	while (cur_thread->sender == -1)
		gthreads_switch();

	if (thrd)
		*thrd = cur_thread->sender;

	res = cur_thread->msg;
	cur_thread->sender = -1;

	return res;
}

int
gthreads_getid(void)
{
	return cur_thread->id;
}

void
gthreads_exit(void)
{
	gthreads_destroy(cur_thread->id);
}


#include <stdio.h>
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
typedef void mkcont_t(); // In order to disable warning


struct thread_info {
	struct thread_info *prev;
	struct thread_info *next;
	ucontext_t ctx;
	gstack_t st;
	int id;
	int delete;

	void *msg;
	int sender;
};


static struct thread_info *head;
static struct thread_info *cur_thread;

static int total_threads = 0;

static timer_t timer;
static int nextid;

static struct itimerspec unarmtm, mintm, tm;
static struct thread_info finaliser;


static int gthreads_arm_timer(void);
static int gthreads_disarm_timer(void);


static void
gthreads_finaliser(void)
{
	free(head);
	exit(0);
}

static void
gthreads_switch_finaliser(void)
{
	/* All or nothing here */
	setcontext(&finaliser.ctx);
}

static int
gthreads_switch_to_thrd(struct thread_info *thrd)
{
	static volatile int passed;

	passed = 0;
	gthreads_disarm_timer();

	if (getcontext(&cur_thread->ctx))
		return 1;

	if (passed > 0)
		return 0;

	passed++;
	cur_thread = thrd;
	gthreads_arm_timer();

	if (setcontext(&cur_thread->ctx)) {
		/* Restore previous thread data */
		cur_thread = cur_thread->prev;
		return 1;
	}

	/* Will never reach */
	return 0;
}

int
gthreads_switch(void)
{
	if (total_threads < 2)
		return 0;

	if (cur_thread->next->delete) {
		gthreads_disarm_timer();
		while (cur_thread->next->delete)
			gthreads_destroy(cur_thread->next->id);
		gthreads_arm_timer();
	}

	return gthreads_switch_to_thrd(cur_thread->next);
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
gthreads_init_finaliser(void)
{
	if (getcontext(&finaliser.ctx))
		return 1;
	stack_init(&finaliser.ctx.uc_stack, &finaliser.st);
	makecontext(&finaliser.ctx, gthreads_finaliser, 0);
	return 0;
}

static int
gthreads_init(void)
{
	if (gthreads_init_timer())
		return 1;

	if (gthreads_init_finaliser())
		return 1;

	if (!(head = malloc(sizeof(struct thread_info))))
		return 1;

	head->sender = -1;
	head->id = 0;
	head->delete = 0;
	head->next = head->prev = head;

	total_threads = 1;
	cur_thread = head;
	nextid = 1;

	return 0;
}

int
gthreads_spawn(gthreads_entry *entry, void *args)
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
	makecontext(&nt->ctx, (mkcont_t *)entry, 1, args);

	nt->id = nextid++;
	nt->delete = 0;
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

	if (total_threads == 1) {
		timer_delete(timer);
		gthreads_switch_finaliser();
		/* All or nothing here */
		exit(0);
	}

	if (cur_thread->id == thrd)
		gthreads_exit();

	gthreads_disarm_timer();
	tp = head;

	do {
		if (tp->id != thrd)
			continue;

		total_threads--;
		tp->next->prev = tp->prev;
		tp->prev->next = tp->next;

		if (head->id == thrd)
			head = head->next;

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
	if (tp->delete)
		return 1;

	/*
	 * TODO: fix situation when tp gets destroyed before it recieves message.
	 */

	/* Waiting for receiver to get all the messages */
	while (tp->sender != -1)
		gthreads_switch_to_thrd(tp);

	tp->sender = cur_thread->id;
	tp->msg = val;

	/* Waiting for receiver to recieve this message */
	while (tp->sender == cur_thread->id)
		gthreads_switch_to_thrd(tp);

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

/*
 * Just for debuging.
 */
static void
gthreads_trace()
{
	struct thread_info *tp;

	tp = head;
	printf("Trace: cur %d\n", cur_thread->id);

	do {
		printf("thread %d %d %d | flag : %d | sender : %d\n",
					tp->prev->id, tp->id, tp->next->id, tp->delete, tp->sender);
	} while ((tp = tp->next) != head);

	printf("END\n");
}

void
gthreads_exit(void)
{
	cur_thread->delete = 1;
//	gthreads_trace();
	gthreads_switch();
}


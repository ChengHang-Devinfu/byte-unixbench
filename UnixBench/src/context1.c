
/*******************************************************************************
 *  The BYTE UNIX Benchmarks - Release 3
 *          Module: context1.c   SID: 3.3 5/15/91 19:30:18
 *
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	Ben Smith, Rick Grehan or Tom Yager
 *	ben@bytepb.byte.com   rick_g@bytepb.byte.com   tyager@bytepb.byte.com
 *
 *******************************************************************************
 *  Modification Log:
 *  $Header: context1.c,v 3.4 87/06/22 14:22:59 kjmcdonell Beta $
 *  August 28, 1990 - changed timing routines--now returns total number of
 *                    iterations in specified time period
 *  October 22, 1997 - code cleanup to remove ANSI C compiler warnings
 *                     Andy Kahn <kahn@zk3.dec.com>
 *
 ******************************************************************************/
char SCCSid[] = "@(#) @(#)context1.c:3.3 -- 5/15/91 19:30:18";
/*
 *  Context switching via synchronized unbuffered pipe i/o
 *
 */

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "timeit.c"

unsigned long iter;

void report()
{
	fprintf(stderr, "COUNT|%lu|1|lps\n", iter);
	exit(0);
}

static int get_cpu_num(void){
	return sysconf(_SC_NPROCESSORS_ONLN);
}

int main(argc, argv)
int	argc;
char	*argv[];
{
	int duration;
	unsigned long	check;
	int	p1[2], p2[2];
	ssize_t ret;
	int need_affinity;

	if (argc != 2) {
		fprintf(stderr, "Usage: context duration\n");
		exit(1);
	}

	duration = atoi(argv[1]);

	/* if os has more than one cpu, will bind parent process to cpu 0 and child process to other cpus
	*	In this way, we can ensure context switch always happen
	* */
	need_affinity = get_cpu_num() >> 1;

	/* set up alarm call */
	iter = 0;
	wake_me(duration, report);
	signal(SIGPIPE, SIG_IGN);

	if (pipe(p1) || pipe(p2)) {
		perror("pipe create failed");
		exit(1);
	}

	if (fork()) {	/* parent process */
		if (need_affinity) {
			cpu_set_t pmask;
			int i;
			CPU_ZERO(&pmask);
			for (i = 0; i < need_affinity; i++)
				CPU_SET(i, &pmask);

			if (sched_setaffinity(0, sizeof(cpu_set_t), &pmask) == -1)
			{
				perror("parent sched_setaffinity failed");
			}
		}

		/* master, write p1 & read p2 */
		close(p1[0]); close(p2[1]);
		while (1) {
			if ((ret = write(p1[1], (char *)&iter, sizeof(iter))) != sizeof(iter)) {
				if ((ret == -1) && (errno == EPIPE)) {
					alarm(0);
					report(); /* does not return */
				}
				if ((ret == -1) && (errno != 0) && (errno != EINTR))
					perror("master write failed");
				exit(1);
			}
			if ((ret = read(p2[0], (char *)&check, sizeof(check))) != sizeof(check)) {
				if ((ret == 0)) { /* end-of-stream */
					alarm(0);
					report(); /* does not return */
				}
				if ((ret == -1) && (errno != 0) && (errno != EINTR))
					perror("master read failed");
				exit(1);
			}
			if (check != iter) {
				fprintf(stderr, "Master sync error: expect %lu, got %lu\n",
					iter, check);
				exit(2);
			}
			iter++;
		}
	}
	else { /* child process */
		if (need_affinity) {
			cpu_set_t pmask;
			int i;
			CPU_ZERO(&pmask);
			for (i = need_affinity; i < (need_affinity << 1); i++)
				CPU_SET(i, &pmask);

			if (sched_setaffinity(0, sizeof(cpu_set_t), &pmask) == -1)
			{
				perror("child sched_setaffinity failed");
			}
		}
		/* slave, read p1 & write p2 */
		close(p1[1]); close(p2[0]);
		while (1) {
			if ((ret = read(p1[0], (char *)&check, sizeof(check))) != sizeof(check)) {
				if ((ret == 0)) { /* end-of-stream */
					alarm(0);
					report(); /* does not return */
				}
				if ((ret == -1) && (errno != 0) && (errno != EINTR))
					perror("slave read failed");
				exit(1);
			}
			if (check != iter) {
				fprintf(stderr, "Slave sync error: expect %lu, got %lu\n",
					iter, check);
				exit(2);
			}
			if ((ret = write(p2[1], (char *)&iter, sizeof(iter))) != sizeof(check)) {
				if ((ret == -1) && (errno == EPIPE)) {
					alarm(0);
					report(); /* does not return */
				}
				if ((ret == -1) && (errno != 0) && (errno != EINTR))
					perror("slave write failed");
				exit(1);
			}
			iter++;
		}
	}
}
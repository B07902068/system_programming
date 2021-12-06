#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "scheduler.h"

#define SIGUSR3 SIGWINCH
int idx;
char arr[10000];
jmp_buf SCHEDULER;
FCB_ptr Current, Head;

int P, Q, T, S, mutex = 0, queue[6] = {};
FCB circular_list[6];
jmp_buf MAIN;
char hash[8] = "01234\0", buf[12];
sigset_t sigset;

void funct(int name);
void dummy(int name)
{
	int a[10000];
	switch (name) {
		case 1:
			funct(1);
			break;
		case 2:
			funct(2);
			break;
		case 3:
			funct(3);
			break;
		case 4:
			funct(4);
			break;
		default:
			fprintf(stderr, "wrong name\n");
			exit(-1);
	}
}
void block_signal()
{
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	sigaddset(&sigset, SIGUSR2);
	sigaddset(&sigset, SIGUSR3);
	if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
		fprintf(stderr, "sigmake error\n");
		exit(-1);
	}
}
static void sig_usr(int signo)
{
//fprintf(stderr, "follow in\n");
	int len = 0;
	sigprocmask(SIG_BLOCK, &sigset, NULL);
	switch (signo) {
		case SIGUSR1: case SIGUSR2:
			printf("C");
			fflush(stdout);
			//longjmp(SCHEDULER, 1);
			break;
		case SIGUSR3:
			Current = Current->Previous;
			for (int i = 1; i <= 4; i++) {
				if (queue[i] == 1) {
					sprintf(&buf[len], "%d ", i);
					len += 2;
				}
			}
			buf[len - 1] = '\n';
			buf[len] = '\0';
			write(STDOUT_FILENO, buf, strlen(buf) + 1);
			break;
		default:
			fprintf(stderr, "wrong signal");
			exit(-1);
	}
	longjmp(SCHEDULER, 1);
}
void funct(int name)
{
	volatile int i = 1, j;
	if (setjmp(circular_list[name].Environment) == 0) {
		if (name == 4) {
			longjmp(MAIN, 1);
		} else {
			dummy(name + 1);
		}
	}
	if (mutex == 0 || mutex == name) {
		mutex = name;
		queue[name] = 0;
		for (; i <= P; i++) {
			for (j = 1; j <= Q; j++) {
				sleep(1);
				arr[idx++] = hash[name]; 
			}
			if (i == P) {
				break;
			}
			if (i % S == 0) {
				mutex = 0;
				i++;
				longjmp(SCHEDULER, 1);
			}
			sigemptyset(&sigset);
			if (sigpending(&sigset) < 0) {
				fprintf(stderr, "pending error\n");
				exit(-1);
			}
			if (sigismember(&sigset, SIGUSR2)) {
				mutex = 0;
			}
			if (sigismember(&sigset, SIGUSR2) || sigismember(&sigset, SIGUSR1) || sigismember(&sigset, SIGUSR3)) {
			//fprintf(stderr, "in\n");
				i++;
				sigprocmask(SIG_UNBLOCK, &sigset, NULL);
			}	
		}
	} else {
		queue[name] = 1;
		longjmp(SCHEDULER, 1);
	}
	mutex = 0;
	longjmp(SCHEDULER, -2);
	
}
void FCB_initialize()
{
	for (int i = 1; i <= 3; i++) {
		circular_list[i].Next = &circular_list[i + 1];
	}
	circular_list[4].Next = &circular_list[1];
	circular_list[1].Previous = &circular_list[4];
	for (int i = 2; i <= 4; i++) {
		circular_list[i].Previous = &circular_list[i - 1];
	}
	for (int i = 1; i <= 4; i++) {
		circular_list[i].Name = i;
	}
	Head = &circular_list[0];
	Head->Next = &circular_list[1];
	Current = Head;
}
void signal_initialize()
{
	struct sigaction act;
	act.sa_handler = sig_usr;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGUSR1, &act, NULL) < 0) {
		fprintf(stderr, "error sigaction\n");
	}
	if (sigaction(SIGUSR2, &act, NULL) < 0) {
		fprintf(stderr, "error sigaction\n");
	}
	if (sigaction(SIGUSR3, &act, NULL) < 0) {
		fprintf(stderr, "error sigaction\n");
	}
	block_signal();
/*	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	sigaddset(&sigset, SIGUSR2);
	sigaddset(&sigset, SIGUSR3);
	if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
		fprintf(stderr, "sigmake error\n");
		exit(-1);
	}*/
}
int main(int argc, char *argv[])
{
	assert(argc == 5);
	P = atoi(argv[1]);
	Q = atoi(argv[2]);
	T = atoi(argv[3]);
	S = atoi(argv[4]);
	FCB_initialize();
	signal_initialize();
	if (T != 2) {
		S = P + 1;	
	}
	if (setjmp(MAIN) == 0) {
		dummy(1);
	}
	Scheduler();
	return 0;
}



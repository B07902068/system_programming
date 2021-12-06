#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SIGUSR3 SIGWINCH

char buf[500];
int main()
{
	int P, Q, R, len, signal[12], pfd[2];
	int hash[5] = {0, SIGUSR1, SIGUSR2, SIGUSR3, 0};
	pid_t pid;
	
	scanf("%d%d%d", &P, &Q, &R);
	for (int i = 0; i < R; i++) {
		scanf("%d", &signal[i]);
	}
	pipe(pfd);

	if ((pid = fork()) < 0) {
		fprintf(stderr, "fork error\n");
		exit(-1);
	} else if (pid == 0) {
		dup2(pfd[1], STDOUT_FILENO);
		close(pfd[1]);
		char p[4], q[4];
		sprintf(p, "%d", P);
		sprintf(q, "%d", Q);
		if (execl("./hw3", "./hw3", p, q, "3", "0", (char *)0) < 0) {
			fprintf(stderr, "exec wrong\n");
			exit(-1);
		}
	}
	for (int i = 0; i < R; i++) {
		sleep(5);
		kill(pid, hash[signal[i]]);
		len = read(pfd[0], buf, sizeof(buf));
		buf[len] = '\0';
		if (signal[i] == 3) {
			printf("%s", buf);
			fflush(stdout);
		}
	}
	len = read(pfd[0], buf, sizeof(buf));
	buf[len] = '\0';
	printf("%s", buf);
	waitpid(pid, NULL, 0);
	close(pfd[0]);
	close(pfd[1]);
	return 0;
}


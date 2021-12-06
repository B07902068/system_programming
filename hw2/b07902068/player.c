#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>



void write_to_parent(int player_id, int money)
{
	fprintf(stdout, "%d %d\n", player_id, money);
	if (fflush(stdout) == EOF) {
		fprintf(stderr, "flush error\n");
		exit(-1);	
	}
	if (fsync(fileno(stdout)) < 0) {		
		/*fprintf(stderr, "out fdopen error\n");
		exit(-1);*/	
	}
	return;
}
int main(int argc, char *argv[])
{
	assert(argc == 2);
	int player_id = atoi(argv[1]), winner;
	write_to_parent(player_id, player_id * 100);	
	for (int i = 0; i < 9; i++) {
		fscanf(stdin, "%d", &winner);
		write_to_parent(player_id, player_id * 100);	
	}

	return 0;
}

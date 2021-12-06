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

typedef struct player {
	int id, score, rank;
} Player;
typedef struct bid {
	int id, money;
} Bid;

void open_fifo(int *infifo_fd, int *outfifo_fd, int host_id, FILE **infifo_fp, FILE **outfifo_fp)
{
	if ((*outfifo_fd = open("Host.FIFO", O_WRONLY)) < 0) {
		fprintf(stderr, "open outfd error\n");
		exit(-1);
	}
	if ((*outfifo_fp = fdopen(*outfifo_fd, "w")) == NULL) {
		fprintf(stderr, "open outfp error\n");
		exit(-1);
	}
	char path[20];
	snprintf(path, sizeof(path), "Host%d.FIFO", host_id);
	if ((*infifo_fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "open infd error\n");
		exit(-1);
	}
	if ((*infifo_fp = fdopen(*infifo_fd, "r")) == NULL) {
		fprintf(stderr, "open infp error\n");
		exit(-1);
	}
	return;
}
void build_pipe(int pfd[][2][2])
{
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			if (pipe(pfd[i][j]) < 0) {
				fprintf(stderr, "pipe error\n");
				exit(-1);
			}
		}	
	}
	/*if (pipe(pfd[2]) < 0) {
		fprintf(stderr, "pipe error\n");
		exit(-1);
	}*/	
	return;
}
void fork_host(int host_id, int random_key, int depth, int pfd[][2][2])
{
	char id[10], key[10], deep[10];
	snprintf(id, sizeof(id), "%d", host_id);
	snprintf(key, sizeof(key), "%d", random_key);
	snprintf(deep, sizeof(deep), "%d", depth + 1);
	int pid;
	for (int i = 0; i < 2; i++) {
		if ((pid = fork()) < 0) {
			fprintf(stderr, "fork error\n");
			exit(-1);
		} else if (pid == 0) {
			close(pfd[i][0][0]);
			close(pfd[i][1][1]);
			dup2(pfd[i][0][1], STDOUT_FILENO);
			dup2(pfd[i][1][0], STDIN_FILENO);
			close(pfd[i][0][1]);
			close(pfd[i][1][0]);

			if (execl("./host", "./host", id, key, deep, (char *)0) < 0) {
				fprintf(stderr, "exec error\n");
				exit(-1);
			}
		}
		close(pfd[i][0][1]);
		close(pfd[i][1][0]);
		
	}
}
void fdopen_pipe(int pfd[][2][2], FILE *pfp[][2])
{
	for (int i = 0; i < 2; i++) {
		if ((pfp[i][0] = fdopen(pfd[i][0][0], "r")) == NULL) {
			fprintf(stderr, "in fdopen error\n");
			exit(-1);	
		}
		if ((pfp[i][1] = fdopen(pfd[i][1][1], "w")) == NULL) {
			fprintf(stderr, "out fdopen error\n");
			exit(-1);	
		}
	}
}
void write_to_child(Player players[], int n, FILE *pfp)
{
	for (int i = 0; i < n; i++) {
		fprintf(pfp, "%d%c", players[i].id, (i == n - 1)? '\n' : ' ');
	}
	if (fflush(pfp) == EOF) {
		fprintf(stderr, "flush error\n");
		exit(-1);	
	}
	if (fsync(fileno(pfp)) < 0) {		
		/*fprintf(stderr, "out fdopen error\n");
		exit(-1);*/	
	}
	return;
}
void receive_bid(FILE *pfp[][2], Bid bids[])
{
	for (int i = 0; i < 2; i++) {
		fscanf(pfp[i][0], "%d %d", &bids[i].id, &bids[i].money);//blocked??? no context switch???
	}
	return;
}
int decide_winner(Bid bids[])
{
	if (bids[0].money >= bids[1].money) {
		return 0;
	}
	return 1;
}
void broadcast_winner(int winner, FILE *pfp[][2]) 
{
	for (int i = 0; i < 2; i++) {
		fprintf(pfp[i][1], "%d\n", winner);
		if (fflush(pfp[i][1]) == EOF) {
			fprintf(stderr, "flush error\n");
			exit(-1);	
		}
		if (fsync(fileno(pfp[i][1])) < 0) {		
			/*fprintf(stderr, "out fdopen error\n");
			exit(-1);*/	
		}
	}
	return;
}
int compare(const void *data1, const void *data2)
{
	Player *ptr1 = (Player *)data1;
	Player *ptr2 = (Player *)data2;
	if (ptr1->score > ptr2->score) {
		return -1;
	}
	return 1;
}
void find_rank(Player players[], int player_num)
{
	Player temp[player_num];
	for (int i = 0; i < player_num; i++) {
		temp[i].id = i;
		temp[i].score = players[i].score;
		temp[i].rank = players[i].rank;
	}
	qsort(temp, player_num, sizeof(Player), compare);
	players[temp[0].id].rank =  1;
	for (int i = 1, rank = 1; i < player_num; i++) {
		if (temp[i].score == temp[i - 1].score) {
			players[temp[i].id].rank = rank;
		} else {
			players[temp[i].id].rank = i + 1;
			rank = i + 1;
		}
	}
	return;
}
void write_result(FILE *outfifo_fp, int random_key, Player players[])
{
	fprintf(outfifo_fp, "%d\n", random_key);
	for (int i = 0; i < 8; i++) {
		fprintf(outfifo_fp, "%d %d\n", players[i].id, players[i].rank);
	}
	if (fflush(outfifo_fp) == EOF) {
		fprintf(stderr, "flush error\n");
		exit(-1);	
	}
	if (fsync(fileno(outfifo_fp)) < 0) {		
		/*fprintf(stderr, "out fdopen error\n");
		exit(-1);*/	
	}
	return;
}
void initialize(Player players[], int n)
{
	for (int i = 0; i < n; i++) {
		players[i].id = 0;
		players[i].score = 0;
		players[i].rank = 0;
	}
}
void wait_child(int n)
{
	for (int i = 0; i < n; i++) {
		if (wait(NULL) < 0) {
			fprintf(stderr, "wait error\n");
			exit(-1);
		}
	}
}
void root_host(int host_id, int random_key, int depth)
{
	int infifo_fd, outfifo_fd;
	FILE *infifo_fp, *outfifo_fp;
	open_fifo(&infifo_fd, &outfifo_fd, host_id, &infifo_fp, &outfifo_fp);

	int pfd[2][2][2];//for left child, parent read from pfd[0][0][0] wirte ro pfd[0][1][1]
	build_pipe(pfd);
	fork_host(host_id, random_key, depth, pfd);
	FILE *pfp[2][2];//for left child, parent read from pfp[0][0], write to pfp[0][1]
	fdopen_pipe(pfd, pfp);
	
	Player players[8] = {};
	Bid bids[2] = {};
	int winner;
	while (1) {
		initialize(players, 8);
		for (int i = 0; i < 8; i++) {
			fscanf(infifo_fp, "%d", &players[i].id);
		}
		write_to_child(players, 4, pfp[0][1]);	
		write_to_child(&players[4], 4, pfp[1][1]);
		if (players[0].id  < 0) {
			break;
		}
		for (int i = 0; i < 10; i++) {
			receive_bid(pfp, bids);		
			winner = bids[decide_winner(bids)].id;
			for (int i = 0; i < 8; i++) {
				if (players[i].id == winner) {
					players[i].score += 1;
					break;
				}
			}			
			broadcast_winner(winner, pfp);
		}
		find_rank(players, 8);
		write_result(outfifo_fp, random_key, players);
	}
	wait_child(2);
	return;
}
void write_to_parent(Bid bid)
{
	fprintf(stdout, "%d %d\n", bid.id, bid.money);
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
void child_host(int host_id, int random_key, int depth)
{
	int pfd[2][2][2];//for left child, parent read from pfd[0][0][0] wirte ro pfd[0][1][1]
	build_pipe(pfd);
	fork_host(host_id, random_key, depth, pfd);
	FILE *pfp[2][2];//for left child, parent read from pfp[0][0], write to pfp[0][1]
	fdopen_pipe(pfd, pfp);
	
	Player players[4] = {};
	Bid bids[2] = {};
	int w, winner;
	while (1) {
		for (int i = 0; i < 4; i++) {
			fscanf(stdin, "%d", &players[i].id);
		}
		write_to_child(players, 2, pfp[0][1]);	
		write_to_child(&players[2], 2, pfp[1][1]);
		if (players[0].id  < 0) {
			break;
		}
		for (int i = 0; i < 10; i++) {
			receive_bid(pfp, bids);		
			w = decide_winner(bids);
			write_to_parent(bids[w]);
			fscanf(stdin, "%d", &winner);
			broadcast_winner(winner, pfp);	
		}
	}
	wait_child(2);
	return;
}
void fork_player(int pfd[][2][2], Player players[])
{
	int pid;
	char id[10];
	for (int i = 0; i < 2; i++) {
		snprintf(id, sizeof(id), "%d", players[i].id);
		if ((pid = fork()) < 0) {
			fprintf(stderr, "fork error\n");
//while(1){} do not use this, must exit after fork error
			exit(-1);
		} else if (pid == 0) {
			close(pfd[i][0][0]);
			close(pfd[i][1][1]);
			dup2(pfd[i][0][1], STDOUT_FILENO);
			dup2(pfd[i][1][0], STDIN_FILENO);
			close(pfd[i][0][1]);
			close(pfd[i][1][0]);

			if (execl("./player", "./player", id, (char *)0) < 0) {
				fprintf(stderr, "exec error\n");
				exit(-1);
			}
		}
		close(pfd[i][0][1]);
		close(pfd[i][1][0]);
			
	}
	return;
}
void close_all(int pfd[][2][2], FILE *pfp[][2])
{
	for (int i = 0; i < 2; i++) {
		close(pfd[i][0][0]);
		close(pfd[i][1][1]);
		fclose(pfp[i][0]);
		fclose(pfp[i][1]);
	}
	return;
}
void leaf_host(int host_id, int random_key)
{
	Player players[2] = {};
	Bid bids[2] = {};
	int w, winner;
	while (1) {	
		for (int i = 0; i < 2; i++) {
			fscanf(stdin, "%d", &players[i].id);
		}	
		if (players[0].id  < 0) {
			break;
		}

		int pfd[2][2][2];//for left child, parent read from pfd[0][0][0] wirte ro pfd[0][1][1]
		build_pipe(pfd);
		fork_player(pfd, players);	
		FILE *pfp[2][2];//for left child, parent read from pfp[0][0], write to pfp[0][1]
		fdopen_pipe(pfd, pfp);
		
		for (int i = 0; i < 10; i++) {
			receive_bid(pfp, bids);		
			w = decide_winner(bids);
			write_to_parent(bids[w]);
			fscanf(stdin, "%d", &winner);
			if (i < 9) {//where is bug, not working for more than 10 players
				broadcast_winner(winner, pfp);	
		
			}
		}
		close_all(pfd, pfp);	
		wait_child(2);
	}


	return;
}
int main(int argc, char *argv[])
{
	assert(argc == 4);
	int host_id = atoi(argv[1]), random_key = atoi(argv[2]), depth = atoi(argv[3]);
	switch (depth) {
	case 0: {
		root_host(host_id, random_key, depth);
		break;
	}
	case 1: {
		child_host(host_id, random_key, depth);
		break;
	}	
	case 2: {
		leaf_host(host_id, random_key);
		break;
	}
	default: {
		fprintf(stderr, "depth wrong\n");
		exit(-1);
	}
	}
	return 0;
}

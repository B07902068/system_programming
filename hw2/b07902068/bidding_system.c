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


#define RWUSR (S_IRUSR | S_IWUSR)
#define RWRWRW (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

typedef struct player{
	int id, score, rank;
} Player;
//reference of listing C(N, 8): https://www.geeksforgeeks.org/print-all-possible-combinations-of-r-elements-in-a-given-array-of-size-n/
//reference of C(N, 8): http://squall.cs.ntou.edu.tw/cprog/practices/Calculate_Comb_n_k_bw.pdf
void do_combination(int data[], int combination[][8], int *count, int start, int end, int index, int r)
{
	if (index == r) {
		for (int i = 0; i < r; i++) {
			combination[*count][i] = data[i];
		}
		*count += 1;
		return;
	}
	for (int i = start; i <= end && end - i  + 1 >= r - index; i++) {
		data[index] = i;
		do_combination(data, combination, count, i+1, end, index+1, r);
	}
	return;
}
int list_combination(int player_num, int combination[][8])
{
	int data[8] = {}, count = 0;
	do_combination(data, combination, &count, 1, player_num, 0, 8);
	for (int i = 0; i < 8; i++) {
		combination[count][i] = -1;
	}
	return count;
}
void print_comb(int combination[][8], int n)
{
	fprintf(stderr, "%d\n", n);
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < 8; j++) {
			fprintf(stderr, "%d ", combination[i][j]);
		}
		fprintf(stderr, "\n");
	}
}
void prepare_fifopath(char fifo_path[][20], int host_num)
{
	size_t size = sizeof(fifo_path[0]);
	snprintf(fifo_path[0], size, "Host.FIFO"); 
	for (int i = 1; i <= host_num; i++) {
		snprintf(fifo_path[i], size, "Host%d.FIFO", i);
	}
	return;
}
void make_FIFO(char fifo_path[][20], int host_num)
{
	for (int i = 0; i <= host_num; i++) {
		if (mkfifo(fifo_path[i], RWUSR)< 0) {
			fprintf(stderr, "mkfifo error\n");
			exit(-1);
		}	
	}
	return;
}
void unlink_FIFO(char fifo_path[][20], int host_num)
{
	for (int i = 0; i <= host_num; i++) {
		if (unlink(fifo_path[i]) < 0) {
			fprintf(stderr, "unlink error\n");
			exit(-1);
		}
	}
	return;
}
void wait_all_host(int host_num)
{
	for (int i = 0; i < host_num; i++) {
		if (wait(NULL) < 0) {
			fprintf(stderr, "wait error");
			exit(-1);
		}
	}
	return;
}
void open_fifo(int fifo_fds[], int host_num, char fifo_path[][20])
{
	if ((fifo_fds[0] = open(fifo_path[0], O_RDWR)) < 0) {
		fprintf(stderr, "open fifo 0 error\n");
		exit(-1);
	}
	for (int i = 1; i <= host_num;) {
		//if ((fifo_fds[i] = open(fifo_path[i], O_WRONLY | O_NONBLOCK)) < 0) {
		if ((fifo_fds[i] = open(fifo_path[i], O_RDWR)) < 0) {
			fprintf(stderr, "open fifo error\n");
			exit(-1);
			//continue;
		}
		i++;
	}
	return;
}
void fdopen_fifo(FILE *fifo_fps[], int host_num, int fifo_fds[])
{
	if ((fifo_fps[0] = fdopen(fifo_fds[0],"r+")) < 0) {
		fprintf(stderr, "fdopen fifo 0 error\n");
		exit(-1);	
	}
	for (int i = 1; i <= host_num; i++) {
		if ((fifo_fps[i] = fdopen(fifo_fds[i],"r+")) == NULL) {
			fprintf(stderr, "fdopen fifo error\n");
			exit(-1);	
		}
	}
	return;
}
void build_host(int host_num, int host_key[])
{
	int pid;
	int used[65536] = {};
	char random_key[20], host_id[10];
	size_t size_ran = sizeof(random_key), size_id = sizeof(host_id);
	srand(time(NULL));
	for (int i = 1; i <= host_num; i++) {
		host_key[i] = rand() % 65536;
		while (used[host_key[i]] == 1) {
			host_key[i] = rand() % 65536;
		}
		used[host_key[i]] = 1;
		snprintf(random_key, size_ran, "%d" ,host_key[i]);
		snprintf(host_id, size_id, "%d" ,i);
		if ((pid = fork()) < 0) {
			fprintf(stderr, "fork error\n");
			exit(-1);
		} else if (pid == 0) {
			if (execl("./host","./host", host_id, random_key, "0", (char*)0) < 0) {
				fprintf(stderr, "exec error\n");
				exit(-1);
			}
		}
	}
	return;
}
void write_to_host(FILE *fifo_fp, int fifo_fd, int message[])
{
	for (int j = 0; j < 8; j++) {//func: write to host
		fprintf(fifo_fp, "%d%c", message[j], (j == 7)? '\n' : ' ');
	}
	if (fflush(fifo_fp) == EOF) {		
		fprintf(stderr, "first fflush error\n");
		exit(-1);
	}
	if (fsync(fifo_fd) < 0) {//cannot fsync a fifo??		
		/*fprintf(stderr, "first fsync error\n");
		fprintf(stderr, "error: %s\n", strerror(errno));
		exit(-1);*/
	}
	return;
}
int score_table[9] = {};
int read_from_host(FILE *fifo_fp, Player players[], int host_key[], int host_num)
{
	int key, id, rank;
	fscanf(fifo_fp, "%d", &key);
	for (int i = 0; i < 8; i++) {
		fscanf(fifo_fp, "%d %d", &id, &rank);
		players[id].score += score_table[rank];
	}
	int host;
	for (int i = 1; i <= host_num; i++) {
		if (key == host_key[i]) {
			host = i;
			break;
		}
	}
	return host;
}
void close_all_host(FILE *fifo_fp[], int fifo_fd[], int host_num, int message[])
{
	assert(message[0] == -1);
	for (int i = 1; i <= host_num; i++) { 
		write_to_host(fifo_fp[i], fifo_fd[i], message);
	}
	return;
}
void initialize(Player players[], int player_num)
{
	for (int i = 1; i <= 8; i++) {
		score_table[i] = 8 - i;
	}
	for (int i = 0; i <= player_num; i++) {
		players[i].id = i;
		players[i].score = 0;
		players[i].rank = 0;
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
	Player temp[player_num + 1];
	for (int i = 0; i <= player_num; i++) {
		temp[i].id = players[i].id;
		temp[i].score = players[i].score;
		temp[i].rank = players[i].rank;
	}
	qsort(&temp[1], player_num, sizeof(Player), compare);
	for (int i = 1, rank = 1; i <= player_num; i++) {
		if (temp[i].score == temp[i - 1].score) {
			players[temp[i].id].rank = rank;
		} else {
			players[temp[i].id].rank = i;
			rank = i;
		}
	}
	return;
}
void bidding(int host_num, int player_num, int com_num, int combination[][8], int fifo_fds[], FILE *fifo_fps[], int host_key[])
{
	Player players[player_num + 1];
	initialize(players, player_num);//also initialize score_table
	//solved: ctrl+c send SIGINT to all foreground process
	
	int c = 0, r = 0;//c count conbinations written to fifo, r count for number of return
	for (int i = 1; i <= host_num && c < com_num; i++) {
		write_to_host(fifo_fps[i], fifo_fds[i], combination[c]);
		c++;
	}
	int next_host;
	while (c < com_num || r < com_num) {
		next_host = read_from_host(fifo_fps[0], players, host_key, host_num);
		r++;
		if (c < com_num) {
			write_to_host(fifo_fps[next_host], fifo_fds[next_host], combination[c]);
			c++;
		}
	}
	close_all_host(fifo_fps, fifo_fds, host_num, combination[com_num]);
	find_rank(players, player_num);	
	for (int i = 1; i <= player_num; i++) {
		printf("%d %d\n", i, players[i].rank);
	}

	return;
}
int main(int argc, char *argv[])
{
	assert(argc == 3);
	int host_num = atoi(argv[1]), player_num = atoi(argv[2]);
	int combination[3005][8] = {};
	int com_num = list_combination(player_num, combination);
	
	char fifo_path[host_num + 2][20];
	prepare_fifopath(fifo_path, host_num);
	make_FIFO(fifo_path, host_num);

	int fifo_fds[host_num + 2];
	FILE *fifo_fps[host_num + 2];
	open_fifo(fifo_fds, host_num, fifo_path);
	fdopen_fifo(fifo_fps, host_num, fifo_fds);
	/*if ((fifo_fds[0] = open(fifo_path[0], O_RDWR)) < 0) {
		fprintf(stderr, "open fifo 0 error\n");
		exit(-1);
	}*/
	int host_key[host_num + 2];
	build_host(host_num, host_key);
/*	open_fifo(fifo_fds, host_num, fifo_path);
		fprintf(stderr, "open success\n");
	fdopen_fifo(fifo_fps, host_num, fifo_fds);*/


	bidding(host_num, player_num, com_num, combination, fifo_fds, fifo_fps, host_key);



	wait_all_host(host_num);
	unlink_FIFO(fifo_path, host_num);
	return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

unsigned char X[60000][784], y[60000][10], X_test[10000][784];
double X_normal[60000][784], X_test_normal[10000][784];
double W[784][10], W_grad[784][10], y_hat[60000][10], Z[60000][10], lr = 0.0000071;
int tnum;

void input(char *argv[])
{
	FILE *Xfp = fopen(argv[1], "rb");
	FILE *yfp = fopen(argv[2], "rb");
	FILE *X_testfp = fopen(argv[3], "rb");
	assert(Xfp != NULL);
	assert(yfp != NULL);
	assert(X_testfp != NULL);
	
	char c;
	for (int i = 0; i < 60000; i++) {
		fread(X[i], sizeof(unsigned char), 784, Xfp);
		for (int j = 0; j < 784; j++) {
			X_normal[i][j] = ((double)X[i][j]) / 255;
		}
		fread(&c, sizeof(unsigned char), 1, yfp);
		y[i][c] = 1;
	}
	for (int i = 0; i < 10000; i++) {
		fread(X_test[i], sizeof(unsigned char), 784, X_testfp);
		for (int j = 0; j < 784; j++) {
			X_test_normal[i][j] = ((double)X_test[i][j]) / 255;
		}
	}
	fclose(Xfp);
	fclose(yfp);
	fclose(X_testfp);
	
	srand(time(NULL));
/*	for (int i = 0; i < 784; i++) {
		for (int j = 0; j < 10; j++) {
			W[i][j] = ((double)rand() / RAND_MAX) / 1000;
		}
	}*/

}
void initialize()
{
	memset(W_grad, 0, sizeof(W_grad));
	memset(y_hat, 0, sizeof(y_hat));
	memset(Z, 0, sizeof(Z));
}
void one_row_multiply(int r, double X_n[][784])
{
	for (int c = 0; c < 10; c++) {
		for (int i = 0; i < 784; i++) {
			Z[r][c] += X_n[r][i] * W[i][c];
			//Z[r][c] += X[r][i] * W[i][c];
		}
	}
}
//void matrix_multiply(int start, int end, double X_n[][784])
void *matrix_multiply(void *arg)
{
	int start = *((int *)arg);
	int end = start + 60000/tnum;
	if (end > 60000) {
		end = 60000;
	}
	for (int r = start; r < end; r++) {
		one_row_multiply(r, X_normal);
	}
	pthread_exit(NULL);
}
void softmax(int R)
{
	double exp_sum;
	for (int r = 0; r < R; r++) {
		exp_sum = 0;
		for (int c = 0; c < 10; c++) {
		//	Z[r][c] /= 10;
			y_hat[r][c] = exp(Z[r][c]);
			exp_sum += y_hat[r][c];
		}
		for (int c = 0; c < 10; c++) {
			y_hat[r][c] /= exp_sum;
			if (R == 60000) {
				y_hat[r][c] -= ((double)y[r][c]);///y_hat - y
		
			}
		}
	}
}
void gradient()
{
	for (int r = 0; r < 784; r++) {
		for (int c = 0; c < 10; c++) {
			for (int i = 0; i < 60000; i++) {
				W_grad[r][c] += X_normal[i][r] * y_hat[i][c];///y_hat = y_hat - y
				//W_grad[r][c] += X[i][r] * y_hat[i][c];///y_hat = y_hat - y
			}
		}
	}
}
void update_W()
{
	for (int r = 0; r < 784; r++) {
		for (int c = 0; c < 10; c++) {
			W[r][c] -= lr * W_grad[r][c];
		}
	}	
}
void train(int n, int tnum)///iteration num
{
	pthread_t tid[tnum];
	int arg[tnum];
	for (int i = 0; i < n; i++) {
		initialize();
		for (int t = 0; t < tnum; t++) {
			arg[t] = t *(60000 / tnum);
			pthread_create(&tid[t], NULL, matrix_multiply, (void *)&arg[t]);	
			//pthread_detach(tid[i]);
			//matrix_multiply(60000, X_normal);
		}
		for (int t = 0; t < tnum; t++) {
			pthread_join(tid[t], NULL);
		}
		softmax(60000);
		gradient();
		update_W();
	}
}
char result[10000]; // temp use
void test()
{
//	FILE *y_testfp = fopen("./y_test", "rb");
//	assert(y_testfp != NULL);
	
	char c;
	initialize();
	//matrix_multiply(10000, X_test_normal);
	for (int r = 0; r < 10000; r++) {
		one_row_multiply(r, X_test_normal);
	}
	softmax(10000);

	int count = 0;
	for (int i = 0, max; i < 10000; i++) {
		max = 0;
		for (int j = 1; j < 10; j++) {
			if (y_hat[i][j] > y_hat[i][max]) {
				max = j;
			}
		}
		result[i] = max;
	/*	fread(&c, sizeof(char), 1, y_testfp);
		if (c == max) {
			count++;
		}*/
	}
//	fclose(y_testfp);

//	printf("%f\n", (double)count/10000);	
	FILE *resultfp = fopen("result.csv", "wb");
	assert(resultfp != NULL);
	fprintf(resultfp, "id,label\n");
	for (int i = 0; i < 10000; i++) {
		fprintf(resultfp, "%d,%d\n", i, result[i]);
	
	}
	fclose(resultfp);
}
int main(int argc, char *argv[])
{
	assert(argc == 5);
	tnum = atoi(argv[4]);///thread num
	input(argv);

	train(6, tnum);
	test();
	
	return 0;
}

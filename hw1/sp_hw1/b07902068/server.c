#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/select.h>

#define ERR_EXIT(a) { perror(a); exit(1); }

#define MAXN 20

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
	int item;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;
typedef struct {
    int id;
    int balance;
} Account;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";

// Forwards

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

static int handle_read(request* reqP);
// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
    struct flock lock;
    lock.l_type = type;
    lock.l_start = offset;
    lock.l_whence = whence;
    lock.l_len = len;
    return (fcntl(fd, cmd, &lock));
}
int main(int argc, char** argv) {
    int i, ret;

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len = sizeof(buf);
    
    int sizeofAccount;
    sizeofAccount = sizeof(Account);
    int locked[MAXN] = {};//reference: B07902084
    
    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Get file descripter table size and initize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
    
#ifdef READ_SERVER
    file_fd = open("./account_list", O_RDONLY, 0);
#else
    file_fd = open("./account_list", O_RDWR, 0);
#endif
    if (file_fd < 0) {
	fprintf(stderr, "open fail\n");
	ERR_EXIT("open fail");
    }

    fd_set listenset, connectingset, readset, secondset, secondreadset;
    FD_ZERO(&listenset);
    FD_ZERO(&connectingset);
    FD_ZERO(&readset);
    FD_ZERO(&secondset);
    FD_ZERO(&secondreadset);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    while (1) {
        // TODO: Add IO multiplexing
        
        // Check new connection
	FD_SET(svr.listen_fd, &listenset);
	select(maxfd, &listenset, NULL, NULL, &timeout);
	
	if (FD_ISSET(svr.listen_fd, &listenset)){
	
	    clilen = sizeof(cliaddr);
            conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
            if (conn_fd < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;  // try again
                if (errno == ENFILE) {
                    (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                    continue;
                }
                ERR_EXIT("accept")
            }
            requestP[conn_fd].conn_fd = conn_fd;
            strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
            fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
	
	    FD_SET(conn_fd, &connectingset);
	}

	memcpy(&readset, &connectingset, sizeof(connectingset));
	select(maxfd, &readset, NULL, NULL, &timeout);
	for (int i = file_fd + 1; i < maxfd && FD_ISSET(i, &connectingset); i++) {
	    if (FD_ISSET(i, &readset)) {
		conn_fd = i;
		if (!FD_ISSET(i, &secondset)) {
		    ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
		    if (ret < 0) {
			fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
			continue;
		    }
		}
	
		int account = atoi(requestP[conn_fd].buf) - 1, account2;
		Account account_info, account2_info;

#ifdef READ_SERVER
		if (locked[account] == 1) {
		    sprintf(buf,"This account is locked.\n");   
		} else if (lock_reg(file_fd, F_SETLK, F_RDLCK, account * sizeofAccount, SEEK_SET, sizeofAccount) < 0) {
		    sprintf(buf,"This account is locked.\n");   
		} else {
		    lseek(file_fd, account * sizeofAccount, SEEK_SET);
		    read(file_fd, &account_info, sizeofAccount);
		
		    sprintf(buf,"%d %d\n",account_info.id, account_info.balance);
		    //sprintf(buf,"%s : %s\n",accept_read_header,requestP[conn_fd].buf);
		}
		write(requestP[conn_fd].conn_fd, buf, strlen(buf));
		close(requestP[conn_fd].conn_fd);
		free_request(&requestP[conn_fd]);
		FD_CLR(conn_fd, &connectingset);
		lock_reg(file_fd, F_SETLK, F_UNLCK, account * sizeofAccount, SEEK_SET, sizeofAccount);
		locked[account] = 0;
#else
		if (!FD_ISSET(conn_fd, &secondset)) {
		    if (locked[account] == 1) {
		        sprintf(buf,"This account is locked.\n");   	
		        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
			close(requestP[conn_fd].conn_fd);
			free_request(&requestP[conn_fd]);
			FD_CLR(conn_fd, &connectingset);	
		    } else if (lock_reg(file_fd, F_SETLK, F_WRLCK, account * sizeofAccount, SEEK_SET, sizeofAccount) < 0) {
		        sprintf(buf,"This account is locked.\n");   	
		        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
			close(requestP[conn_fd].conn_fd);
			free_request(&requestP[conn_fd]);
			FD_CLR(conn_fd, &connectingset);	
		    } else { 
			locked[account] = 1;
		        sprintf(buf,"This account is modifiable.\n");   	
		        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
		        FD_SET(conn_fd, &secondset);
		    }
		} else {
		        lseek(file_fd, account * sizeofAccount, SEEK_SET);
		        read(file_fd, &account_info, sizeofAccount);

		        ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
			if (ret < 0) {
			    fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
			    continue;
		        }
			int num;
			switch (requestP[conn_fd].buf[0]) {
			    case 's':
				num = atoi(strrchr(requestP[conn_fd].buf, ' '));
				if (num < 0) {
				    sprintf(buf, "Operation failed.\n");
				} else {
				    account_info.balance += num;
				}
				break;
			    case 'w':
				num = atoi(strrchr(requestP[conn_fd].buf, ' '));
				if (num < 0 || num > account_info.balance) {
				    sprintf(buf, "Operation failed.\n");
				    num = -1;
				} else {
				    account_info.balance -= num;
				}
				break;
			    case 't':
				num = atoi(strrchr(requestP[conn_fd].buf, ' '));
				account2 = atoi(strchr(requestP[conn_fd].buf, ' ')) - 1;
				if (num < 0 || num > account_info.balance) {
				    sprintf(buf, "Operation failed.\n");
				    num = -1;
				} else {
		        	    lseek(file_fd, account2 * sizeofAccount, SEEK_SET);
		        	    read(file_fd, &account2_info, sizeofAccount);
				    account_info.balance -= num;
				    account2_info.balance += num;
		        	    lseek(file_fd, account2 * sizeofAccount, SEEK_SET);
				    write(file_fd, &account2_info, sizeofAccount);	
				}
				break;
			    case 'b':
				num = atoi(strrchr(requestP[conn_fd].buf, ' '));
				if (num < 0) {
				    sprintf(buf, "Operation failed.\n");
				} else {
				    account_info.balance = num;
				}
				break;
			    default:
				break;
			}
			
		        lseek(file_fd, account * sizeofAccount, SEEK_SET);
			write(file_fd, &account_info, sizeofAccount);	
		//sprintf(buf,"%s : %s\n",accept_write_header,requestP[conn_fd].buf);
		    if (num < 0) {
			write(requestP[conn_fd].conn_fd, buf, strlen(buf));
		    }
		    close(requestP[conn_fd].conn_fd);
		    free_request(&requestP[conn_fd]);
		    FD_CLR(conn_fd, &connectingset);	
		    FD_CLR(conn_fd, &secondset);	
		    lock_reg(file_fd, F_SETLK, F_UNLCK, account * sizeofAccount, SEEK_SET, sizeofAccount);
		    locked[account] = 0;
		}
#endif

	    }
	}
	
    }
    free(requestP);
    
    return 0;
}


// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void* e_malloc(size_t size);


static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->item = 0;
    reqP->wait_for_write = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error
static int handle_read(request* reqP) {
    int r;
    char buf[512];

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
	char* p1 = strstr(buf, "\015\012");
	int newline_len = 2;
	// be careful that in Windows, line ends with \015\012
	if (p1 == NULL) {
		p1 = strstr(buf, "\012");
		newline_len = 1;
		if (p1 == NULL) {
			ERR_EXIT("this really should not happen...");
		}
	}
	size_t len = p1 - buf + 1;
	memmove(reqP->buf, buf, len);
	reqP->buf[len - 1] = '\0';
	reqP->buf_len = len-1;
    return 1;
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }
}

static void* e_malloc(size_t size) {
    void* ptr;

    ptr = malloc(size);
    if (ptr == NULL) ERR_EXIT("out of memory");
    return ptr;
}


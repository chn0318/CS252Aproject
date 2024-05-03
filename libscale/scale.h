#ifndef SCALE_H
#define SCALE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <malloc.h>
#include <errno.h>
#include <byteswap.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <getopt.h>
#include"log.h"
#include "scale_type.h"
#include "verbs.h"


struct sock_with_lock
{
	int sock;
	pthread_mutex_t mutex;
};

struct scale_mr
{
	struct ibv_mr mr;
	char shm_name[100];
	void* shm_ptr;
	int shm_fd;
	//only vaild in router memory space
	void* router_mr;
	void* router_addr;
	int router_shm_fd;
};

struct scale_qp{
	struct ibv_qp qp;
	uint32_t dst_v_qp_num;
};

void init_unix_sock();
struct sock_with_lock* get_unix_sock(SCALE_FUNCTION_CALL req);
void connect_router(struct sock_with_lock *unix_sock);
int request_router(SCALE_FUNCTION_CALL req, void* req_body, void *rsp, int *rsp_size);

#endif
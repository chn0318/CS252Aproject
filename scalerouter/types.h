#ifndef TYPES_H
#define TYPES_H

#include <infiniband/verbs.h>

#include <rdma/rdma_cma.h>
#include <rdma/rdma_cma_abi.h>

#include <rdma/rdma_verbs.h>
#include <infiniband/kern-abi.h>

#include <pthread.h>

/* types for router only */
// #define MAX_HOST_NUM 1024

#define MAX_POLL_CQ_TIMEOUT 5000
#define NUM_REPEATS 10

#define MAX_RECV_SGE 5
#define MAX_SEND_SGE 5
#define MAX_RECV_WR 1024
#define MAX_SEND_WR 1024
#define MAX_CQ_SIZE 1024


// #define PORT 51000
// #define IB_PORT_NUM 1
// #define POLL_TIMEOUT_MS 5000
// #define NUM_REPEATS 10
// #define MSG_SIZE 768
// #define MAX_SIZE 4096
// #define MAP_SIZE 10240

// #define CTRL_REQ_SIZE 1024 * 1024 // 1 MB
// #define CTRL_RSP_SIZE 1024 * 1024 // 1 MB


#endif /* TYPES_H */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <malloc.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include <infiniband/verbs.h>

#define MSG      "SEND operation      "
#define MSG_SIZE (strlen(MSG) + 1)
struct pingpong_context {
	struct ibv_context	*context;
	struct ibv_pd		*pd;
	struct ibv_mr		*mr;
	struct ibv_cq		*cq;
	struct ibv_qp		*qp;
	void			*buf;
	unsigned long long	 size;
	int			 rx_depth;
	int			 pending;
	struct ibv_port_attr	 portinfo;
	int			 inlr_recv;
};

struct pingpong_dest {
	int lid;
	int qpn;
	int psn;
	union ibv_gid gid;
};
void wire_gid_to_gid(const char *wgid, union ibv_gid *gid)
{
	char tmp[9];
	__be32 v32;
	int i;
	uint32_t tmp_gid[4];

	for (tmp[8] = 0, i = 0; i < 4; ++i) {
		memcpy(tmp, wgid + i * 8, 8);
		sscanf(tmp, "%x", &v32);
		tmp_gid[i] = be32toh(v32);
	}
	memcpy(gid, tmp_gid, sizeof(*gid));
}

void gid_to_wire_gid(const union ibv_gid *gid, char wgid[])
{
	uint32_t tmp_gid[4];
	int i;

	memcpy(tmp_gid, gid, sizeof(tmp_gid));
	for (i = 0; i < 4; ++i)
		sprintf(&wgid[i * 8], "%08x", htobe32(tmp_gid[i]));
}


static struct pingpong_context * init_ctx(struct ibv_device *ib_dev, unsigned long long size, int rx_depth, int port){
    struct pingpong_context *ctx;
    ctx=calloc(1,sizeof *ctx);
    ctx->size=size;
    ctx->rx_depth=rx_depth;
    ctx->buf=memalign(sysconf(_SC_PAGESIZE),size);
    if (!ctx->buf) {
		fprintf(stderr, "Couldn't allocate work buf.\n");
		goto clean_ctx;
	}

    ctx->context = ibv_open_device(ib_dev);
    if (!ctx->context) {
		fprintf(stderr, "Couldn't get context\n");
		goto clean_buffer;
	}

    ctx->pd = ibv_alloc_pd(ctx->context);
    if (!ctx->pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		goto clean_device;
	}

    int access_flags= IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, size, access_flags);
    if (!ctx->pd) {
		fprintf(stderr, "Couldn't register MR\n");
		goto clean_pd;
	}

    ctx->cq = ibv_create_cq(ctx->context, rx_depth+1, NULL, NULL, 0);
    if(!ctx->cq){
        fprintf(stderr, "Couldn't create CQ\n");
        goto clean_mr;
    }

	struct ibv_qp_init_attr init_attr = {
		.send_cq = ctx->cq,
		.recv_cq = ctx->cq,
		.cap     = {		
			.max_send_wr  = 1,
			.max_recv_wr  = rx_depth,
			.max_send_sge = 1,
			.max_recv_sge = 1
		},
		.qp_type = IBV_QPT_RC
	};
    ctx->qp = ibv_create_qp(ctx->pd, &init_attr);
    if(!ctx->qp){
        fprintf(stderr,"Couldn't create QP\n");
        goto clean_cq;
    }

    struct ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_INIT,
			.pkey_index      = 0,
			.port_num        = port,
			.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
	};

	if (ibv_modify_qp(ctx->qp, &attr,
			  IBV_QP_STATE              |
			  IBV_QP_PKEY_INDEX         |
			  IBV_QP_PORT               |
			  IBV_QP_ACCESS_FLAGS)) {
		fprintf(stderr, "Failed to modify QP to INIT\n");
		goto clean_qp;
	}

    return ctx;

clean_qp:
	ibv_destroy_qp(ctx->qp);

clean_cq:
	ibv_destroy_cq(ctx->cq);

clean_mr:
	ibv_dereg_mr(ctx->mr);

clean_pd:
	ibv_dealloc_pd(ctx->pd);

clean_device:
	ibv_close_device(ctx->context);

clean_buffer:
	free(ctx->buf);

clean_ctx:
	free(ctx);

	return NULL;
}
static struct pingpong_dest* exchange_meta_data(char* servername, int port, struct pingpong_dest* my_dest){
	struct pingpong_dest* rem_dest;
	struct addrinfo *resolved_addr=NULL;
	struct addrinfo * itr;
	char service[6];
	int sockfd = -1;
	int listenfd=0;
	int reuse_addr=1;
	int tmp;
	struct addrinfo hints =
	{
		.ai_flags = AI_PASSIVE,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};
	sprintf(service, "%d", port);

	sockfd = getaddrinfo(servername, service, &hints, &resolved_addr);
	if (sockfd < 0) {
		fprintf(stderr,"Couldn't resolve DNS address\n");
		freeaddrinfo(resolved_addr);
		return NULL;
	}
	for(itr=resolved_addr;itr;itr=itr->ai_next){
		sockfd = socket(itr->ai_family, itr->ai_socktype, itr->ai_protocol);
		if(sockfd>=0){
			if(servername){
				printf("Waiting to connect to server\n");
				if((tmp = connect(sockfd, itr->ai_addr, itr->ai_addrlen))){
					close(sockfd);
					sockfd=-1;
				}
			}
			else{
				listenfd = sockfd;
				sockfd=-1;
				setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int));
				if(bind(listenfd, itr->ai_addr, itr->ai_addrlen)){
					freeaddrinfo(resolved_addr);
					return NULL;
				}
				listen(listenfd, 1);
				printf("Waiting to hear from client\n");
				sockfd = accept(listenfd, NULL, 0);

			}
		}
	}
	char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
	char gid[33];
	if(servername){
		gid_to_wire_gid(&my_dest->gid, gid);
		sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn,my_dest->psn, gid);
		if (write(sockfd, msg, sizeof msg) != sizeof msg) {
			fprintf(stderr, "Couldn't send local address\n");
			goto out;
		}

		if (read(sockfd, msg, sizeof msg) != sizeof msg ||write(sockfd, "done", sizeof "done") != sizeof "done") {
			perror("client read/write");
			fprintf(stderr, "Couldn't read/write remote address\n");
			goto out;
		}
		rem_dest = malloc(sizeof *rem_dest);
		if (!rem_dest)
			goto out;
		sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn,&rem_dest->psn, gid);
		wire_gid_to_gid(gid, &rem_dest->gid);
	}
	else{
		if (read(sockfd, msg, sizeof msg) != sizeof msg) {
			fprintf(stderr, "Couldn't read remote address\n");
			goto out;
		}
		rem_dest = malloc(sizeof *rem_dest);
		if (!rem_dest)
			goto out;
		sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn,&rem_dest->psn, gid);
		wire_gid_to_gid(gid, &rem_dest->gid);

		gid_to_wire_gid(&my_dest->gid, gid);
		sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn,my_dest->psn, gid);
		if (write(sockfd, msg, sizeof msg) != sizeof msg ||read(sockfd, msg, sizeof msg) != sizeof "done") {
			fprintf(stderr, "Couldn't send/recv local address\n");
			free(rem_dest);
			rem_dest = NULL;
			goto out;
		}
	}

out:
	close(sockfd);
	return rem_dest;	
}
int main(int argc, char* argv[]){
	struct ibv_device      **dev_list;
	struct ibv_device	*ib_dev;
	struct pingpong_context *ctx;
	struct pingpong_dest     my_dest;
	struct pingpong_dest    *rem_dest;
    int ib_port=1;
	int tcp_port=19875;
	int gid_idx=3;
    int rx_depth=1024;
    unsigned long long size= 4096;
	char* servername=NULL;
	if(argc==2){
		servername=argv[1];
	}
	printf("------Step1: init res------\n");
    dev_list=ibv_get_device_list(NULL);
    if (!dev_list) {
		perror("Failed to get IB devices list");
		return 1;
	}

    ib_dev = *dev_list;
	if (!ib_dev) {
		fprintf(stderr, "No IB devices found\n");			
        return 1;
	}

    ctx=init_ctx(ib_dev,size,rx_depth,ib_port);
    if(!servername){
		struct ibv_recv_wr rr;
		struct ibv_sge sge;
		struct ibv_recv_wr *bad_wr = NULL;
		memset(&sge, 0, sizeof(sge));
		sge.addr = (uintptr_t)ctx->buf;
		sge.length = MSG_SIZE;
		sge.lkey = ctx->mr->lkey;
		memset(&rr, 0, sizeof(rr));
		rr.next = NULL;
		rr.wr_id = (int)getpid();
		rr.sg_list = &sge;
		rr.num_sge = 1;
        if(ibv_post_recv(ctx->qp, &rr, &bad_wr)){
			fprintf(stderr, "post recv failed\n");
			return 1;
		};

    }
    if(ibv_query_port(ctx->context, ib_port, &ctx->portinfo)){
        fprintf(stderr, "Couldn't get port info\n");
        return 1;
    }
    my_dest.lid = ctx->portinfo.lid;
	if (ibv_query_gid(ctx->context, ib_port, gid_idx, &my_dest.gid)) {
		fprintf(stderr, "Couldn't read sgid of index\n");
		return 1;
	}

	my_dest.qpn = ctx->qp->qp_num;
	my_dest.psn = lrand48() & 0xffffff;
	char			 gid[33];
	inet_ntop(AF_INET6, &my_dest.gid, gid, sizeof gid);
	printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
	       my_dest.lid, my_dest.qpn, my_dest.psn, gid);

    //socket带外建链,交换meta data: 交换lid，gid，qpn
    //this step is transparent to RDMA
	printf("------Step2: exchage meta data------\n");
    rem_dest=exchange_meta_data(servername, tcp_port, &my_dest);
	inet_ntop(AF_INET6, &rem_dest->gid, gid, sizeof gid);
	printf("  remote address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
	       rem_dest->lid, rem_dest->qpn, rem_dest->psn, gid);
    //建立连接, 修改QP状态
	printf("-----Step3: modify state of QP------\n");
    struct ibv_qp_attr attr;
	int flags;
	memset(&attr, 0, sizeof(attr));
	attr.qp_state = IBV_QPS_RTR;
	attr.path_mtu = IBV_MTU_256;
	attr.dest_qp_num = rem_dest->qpn;
	attr.rq_psn = 0;
	attr.max_dest_rd_atomic = 16;
	attr.min_rnr_timer = 0x12;
	attr.ah_attr.is_global = 0;
	attr.ah_attr.dlid = rem_dest->lid;
	attr.ah_attr.sl = 0;
	attr.ah_attr.src_path_bits = 0;
	attr.ah_attr.port_num = ib_port;
	if (gid_idx >= 0) {
		attr.ah_attr.is_global = 1;
		attr.ah_attr.port_num = 1;
		attr.ah_attr.grh.dgid = rem_dest->gid;
		attr.ah_attr.grh.flow_label = 0;
		attr.ah_attr.grh.hop_limit = 0xff;
		attr.ah_attr.grh.sgid_index = gid_idx;
		attr.ah_attr.grh.traffic_class = 0;
	}
	flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
		IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
	if(ibv_modify_qp(ctx->qp, &attr, flags)){
		fprintf(stderr, "failed to modify QP state to RTR\n");
	}

	attr.qp_state = IBV_QPS_RTS;
	attr.timeout = 0x12;
	attr.retry_cnt = 6;
	attr.rnr_retry = 0;
	attr.sq_psn = 0;
	attr.max_rd_atomic = 16; // number of outstanding wqe for RDMA read
	flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
		IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
	if(ibv_modify_qp(ctx->qp, &attr, flags)){
		fprintf(stderr, "failed to modify QP state to RTS\n");
	}
    //提交工作请求
	printf("-----Step4: Send MSG------\n");
    if(servername){
        strcpy((char *)ctx->buf, MSG);
		printf("The msg is %s\n",(char *)ctx->buf);
       	struct ibv_send_wr sr;
		struct ibv_sge sge;	
		struct ibv_send_wr *bad_wr = NULL;
		memset(&sge, 0, sizeof(sge));
		sge.addr = (uintptr_t)ctx->buf;
		sge.length = MSG_SIZE;
		sge.lkey = ctx->mr->lkey;
		memset(&sr, 0, sizeof(sr));
		sr.next = NULL;
		sr.wr_id = (int)getpid();
		sr.sg_list = &sge;
		sr.num_sge = 1;
		sr.opcode = IBV_WR_SEND;
		sr.send_flags = IBV_SEND_SIGNALED;
        if(ibv_post_send(ctx->qp, &sr, &bad_wr)){
			fprintf(stderr, "post send failed\n");
			return 1;
		};
    }
    
    printf("-----Step5: Poll WC------\n");
    int poll_result, i;
	struct ibv_wc* wc;
	wc = (struct ibv_wc *)calloc(1, sizeof(struct ibv_wc));
	do {
		poll_result = ibv_poll_cq(ctx->cq, 1, wc);
		if (poll_result < 0) {
			fprintf(stderr, "poll CQ failed\n");
			return 1;
		}
	} while (poll_result < 1);
	printf("Get %d WC\n",poll_result);
    for(i = 0; i < poll_result; i++){
        if (wc[i].status != IBV_WC_SUCCESS){
            fprintf(stderr,"got bad completion with status: 0x%x, vendor syndrome: 0x%x\n", wc[i].status, wc[i].vendor_err);
            return 1;
		}
		if (wc[i].wr_id != (int)getpid()){
			fprintf(stderr,"reqeust header process_id is not equal to wr_id\n");
			return 1;
		}
	}

    if(servername){
        printf("[Client] SEND message is: '%s'\n", (char *)ctx->buf);
    }
    else{
        printf("[Server] RECV message is: '%s'\n", (char *)ctx->buf);
    }


	if (ibv_destroy_qp(ctx->qp)) {
		fprintf(stderr, "Couldn't destroy QP\n");
		return 1;
	}

	if (ibv_destroy_cq(ctx->cq)) {
		fprintf(stderr, "Couldn't destroy CQ\n");
		return 1;
	}

	if (ibv_dereg_mr(ctx->mr)) {
		fprintf(stderr, "Couldn't deregister MR\n");
		return 1;
	}

	if (ibv_dealloc_pd(ctx->pd)) {
		fprintf(stderr, "Couldn't deallocate PD\n");
		return 1;
	}

	if (ibv_close_device(ctx->context)) {
		fprintf(stderr, "Couldn't release context\n");
		return 1;
	}
    ibv_free_device_list(dev_list);
	free(rem_dest);
    return 0;
}
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <search.h>
#include <linux/ip.h>
#include <netinet/in.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include "verbs.h"
#include "scale.h"
/*
ibv_get_device_list()通过UDS向Router请求ibv_device。该版本的功能比较简单，返回的device_list仅仅含有一个元素，该元素是router选择并使用的device
保存在router->global_res->ib_dev中。之前的版本，dev_list和ib_dev在init_global_res()的时候就被free了，但是在此版本中，我将device_list和ib_dev都加入了
global_res中，只有在调用destroy_global_res之后才会进行ibv_free_device_list()的操作，进行资源回收。
*/

struct {
	uint16_t cq_refcnt;
	pthread_mutex_t mutex;
}cq_refcnt_with_lock;

struct {
	uint16_t qp_refcnt;
	pthread_mutex_t mutex;
}qp_refcnt_with_lock;

uint16_t cq_qp_map[1024];

void scale_get_router_addr(struct ibv_mr *mr, void** router_addr){
    struct scale_mr* s_mr=container_of(mr, struct scale_mr, mr);
    *router_addr = s_mr->router_addr;
}

//当前版本仅支持一个wr，一个sge
int	post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr){
    struct IBV_POST_SEND_REQ req;
    memset(&req, 0, sizeof(req));
    req.qp_handle=qp->handle;
    req.lkey=wr->sg_list->lkey;
    req.wr_id=wr->wr_id;
    req.client_addr=(void*)wr->sg_list->addr;
    req.length=wr->sg_list->length;
    req.opcode=wr->opcode;
    if(req.opcode != IBV_WR_SEND){
        req.remote_router_addr = (void*)wr->wr.rdma.remote_addr;
        req.rkey = wr->wr.rdma.rkey;
	} 
    struct IBV_POST_SEND_RSP rsp;
    int rsp_size= sizeof(struct IBV_POST_SEND_RSP);
    int rc = request_router(IBV_POST_SEND,&req, (void*)&rsp, &rsp_size);
    if(rc==-1){
        LOG_ERROR_PRINTF("Fail to request router when post send");
        return -1;
    }
    if(rsp.wr_id!=(int)getpid()){
        LOG_ERROR_PRINTF("Some sync error may happen");
        return -1;
    }
    return 0;
}

int post_recv(struct ibv_qp *qp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr){
    struct IBV_POST_RECV_REQ req;
    memset(&req, 0 , sizeof(struct IBV_POST_RECV_REQ));
    req.qp_handle = qp->handle;
    req.length = wr->sg_list->length;
    req.lkey = wr->sg_list->lkey;
    req.wr_id=wr->wr_id;
    req.client_addr=(void*) wr->sg_list->addr;
    struct IBV_POST_RECV_RSP rsp;
    int rsp_size = sizeof(struct IBV_POST_RECV_RSP);
    int rc = request_router(IBV_POST_RECV, &req, (void*)&rsp, &rsp_size);
    if(rc==-1){
        LOG_ERROR_PRINTF("Fail to request router when post recv");
        return -1;
    }
    if(rsp.wr_id!=(int)getpid()){
        LOG_ERROR_PRINTF("Some sync error may happen");
        return -1;
    }
    return 0;

}

int poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc){
    if(num_entries!=1){
        LOG_ERROR_PRINTF("In current version, router only support poll 1 cqe once a time");
        return -1;
    }
    pid_t process_id = getpid();
    struct IBV_POLL_CQ_REQ req;
    req.cq_handle=cq->handle;
    req.qp_handle= ((uint32_t)process_id << 16)|cq_qp_map[cq->handle & 0xFFFF];
    req.num_entries =num_entries;
    struct IBV_POLL_CQ_RSP rsp;
    int rsp_size=sizeof(struct IBV_POLL_CQ_RSP);
    int rc = request_router(IBV_POLL_CQ, &req, (void*)&rsp, &rsp_size);
    if(rc==-1){
        LOG_ERROR_PRINTF("Fail to request router when poll cq");
        return -1;
    }
    wc->status = rsp.status;
    wc->opcode = rsp.opcode;
    wc->wr_id = rsp.wr_id;
    return rsp.count;

}

struct ibv_device **ibv_get_device_list(int *num){
    struct ibv_device **l;

    l=calloc(1, sizeof (struct ibv_device*));
    *l=calloc(1,sizeof(struct ibv_device));

    void* req_body=NULL;
    struct IBV_GET_DEVICE_LIST_RSP rsp;
    int rsp_size=sizeof(struct IBV_GET_DEVICE_LIST_RSP);

    int rc=request_router(IBV_GET_DEVICE_LIST, req_body, (void*)&rsp, &rsp_size);
    if(rc==-1){
        LOG_ERROR_PRINTF("Fail to request router\n");
        return NULL;
    }
    if(num){
        *num=1;
    }
    l[0]->node_type=rsp.node_type;
    l[0]->transport_type=rsp.transport_type;
    strcpy(l[0]->dev_name,rsp.dev_name);
    strcpy(l[0]->dev_path,rsp.dev_path);
    strcpy(l[0]->ibdev_path,rsp.ibdev_path);
    strcpy(l[0]->name,rsp.name);
    pthread_mutex_init(&cq_refcnt_with_lock.mutex,NULL);
    pthread_mutex_init(&qp_refcnt_with_lock.mutex,NULL);
    memset(cq_qp_map, 0, 1024*sizeof(uint16_t));
    return l;
}

const char* ibv_get_device_name(struct ibv_device* device){
    return device->name;
}

struct ibv_context *ibv_open_device(struct ibv_device *device){
    struct ibv_context* ctx;
    ctx=calloc(1,sizeof(struct ibv_context));
    ctx->device=device;
    ctx->cmd_fd=1;
    ctx->ops.post_send=post_send;
    ctx->ops.post_recv=post_recv;
    ctx->ops.poll_cq= poll_cq;
    pthread_mutex_init(&ctx->mutex, NULL);
    return ctx;    
}

struct ibv_pd *ibv_alloc_pd(struct ibv_context *context){
    struct ibv_pd* pd;
    pd=calloc(1,sizeof(struct ibv_pd));
    pd->context=context;
    //感觉pd->handle是否赋值并不重要，为了兼容性考虑，向router发出请求，获得后端的pd->handle
    void* req_body=NULL;
    struct IBV_ALLOC_PD_RSP rsp;
    int rsp_size=sizeof(struct IBV_ALLOC_PD_RSP);
    int rc=request_router(IBV_ALLOC_PD,req_body,(void*)&rsp,&rsp_size);
    if (rc==-1)
    {
        LOG_ERROR_PRINTF("Fail to request router when alloc pd\n");
        return NULL;
    }
    pd->handle=rsp.handle;
    return pd;
}

struct ibv_mr *ibv_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access){
    //判断addr是否页对齐
    int is_align = (long) addr % (4*1024)==0?1:0;
    if(!is_align){
        LOG_ERROR_PRINTF("In current version, client address must be page aligned\n");
        return NULL;
    }      
    struct scale_mr* s_mr=calloc(1,sizeof(struct scale_mr));
    //转发请求
    struct IBV_REG_MR_REQ req;
    req.access_flag=access;
    req.length=length;
    req.client_addr=addr;
    struct IBV_REG_MR_RSP rsp;
    int rsp_size=sizeof(struct IBV_REG_MR_RSP);
    int rc=request_router(IBV_REG_MR,&req,(void*)&rsp,&rsp_size);
    if(rc==-1){
        LOG_ERROR_PRINTF("Fail to request router when register mr\n");
        return NULL;
    }

    strcpy(s_mr->shm_name,rsp.shm_name);
    char* buf=(char* )malloc(length);
    memcpy(buf, addr, length);
    int ret;
    s_mr->shm_fd=shm_open(s_mr->shm_name,O_CREAT | O_RDWR, 0666);
    ret = ftruncate(s_mr->shm_fd, length);
    s_mr->shm_ptr=mmap(addr, length, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_LOCKED, s_mr->shm_fd, 0);
    if (s_mr->shm_ptr == MAP_FAILED || ret > 0){
		LOG_ERROR_PRINTF("mmap failed in REG_MR.\n");
        free(buf);
        return NULL;
	}
    //正确性检验，确保共享内存地址和client addr是一致的
    memcpy(addr,buf,length);
    free(buf);
    if(s_mr->shm_ptr!=addr){
        LOG_ERROR_PRINTF("The shm addr dose not match with client addr\n");
        return NULL;
    }
    LOG_TRACE_PRINTF("The router addr is 0x%x\n", (uintptr_t)rsp.router_addr);
    s_mr->router_shm_fd=rsp.router_shm_fd;
    s_mr->router_mr = rsp.router_mr;
    s_mr->router_addr=rsp.router_addr;
    s_mr->mr.addr=addr;
    s_mr->mr.context=pd->context;
    s_mr->mr.handle=rsp.handle;
    s_mr->mr.length=length;
    s_mr->mr.pd=pd;
    s_mr->mr.lkey=rsp.lkey;
    s_mr->mr.rkey=rsp.rkey;

    return &(s_mr->mr);
}

//为了版本兼容性考虑
struct ibv_mr *ibv_reg_mr_iova2(struct ibv_pd *pd, void *addr, size_t length,
				uint64_t iova, unsigned int access){
    return ibv_reg_mr(pd,addr,length,access);
}

int ibv_query_gid(struct ibv_context *context, uint8_t port_num, int index, union ibv_gid *gid){
    void* req_body=NULL;
    struct IBV_QUERY_GID_RSP rsp;
    int rsp_size=sizeof(struct IBV_QUERY_GID_RSP);
    int rc=request_router(IBV_QUERY_GID,req_body,(void*)&rsp,&rsp_size);
    if(rc==-1){
        LOG_ERROR_PRINTF("Fail to request router when query gid\n");
        return -1;
    }
    memcpy(gid,rsp.gid,sizeof(*gid));
    return 0;
}

int ibv_query_port(struct ibv_context *context, uint8_t port_num,
		   struct ibv_port_attr *port_attr){
    void* req_body=NULL;
    struct IBV_QUERY_PORT_RSP rsp;
    int rsp_size=sizeof(struct IBV_QUERY_PORT_RSP);
    int rc=request_router(IBV_QUERY_PORT,req_body,(void*)&rsp,&rsp_size);
    if (rc==-1)
    {
        LOG_ERROR_PRINTF("Fail to request router when query port\n");
        return -1;
    }
    port_attr->active_mtu=rsp.active_mtu;
    port_attr->active_speed=rsp.active_speed;
    port_attr->active_width=rsp.active_width;
    port_attr->bad_pkey_cntr=rsp.bad_pkey_cntr;
    port_attr->gid_tbl_len=rsp.gid_tbl_len;
    port_attr->init_type_reply=rsp.init_type_reply;
    port_attr->lid=rsp.lid;
    port_attr->lmc=rsp.lmc;
    port_attr->max_msg_sz=rsp.max_msg_sz;
    port_attr->max_mtu=rsp.max_mtu;
    port_attr->max_vl_num=rsp.max_vl_num;
    port_attr->phys_state=rsp.phys_state;
    port_attr->pkey_tbl_len=rsp.pkey_tbl_len;
    port_attr->port_cap_flags=rsp.port_cap_flags;
    port_attr->qkey_viol_cntr=rsp.qkey_viol_cntr;
    port_attr->sm_lid=rsp.sm_lid;
    port_attr->sm_sl=rsp.sm_sl;
    port_attr->state=rsp.state;
    port_attr->subnet_timeout=rsp.subnet_timeout;
    port_attr->link_layer=rsp.link_layer;
    return 0;           
}

struct ibv_cq *ibv_create_cq(struct ibv_context *context, int cqe, void *cq_context, struct ibv_comp_channel *channel, int comp_vector){
    struct ibv_cq* cq;
    cq=calloc(1,sizeof(struct ibv_cq));
    cq->cqe=cqe;
    cq->context=context;
    cq->channel=channel;
    cq->cq_context=cq_context;
    cq->comp_events_completed  = 0;	
	cq->async_events_completed = 0; 
	pthread_mutex_init(&cq->mutex, NULL);
	pthread_cond_init(&cq->cond, NULL);

    //使用pid:cq_refcnt作为cq_handle,这样保证了cq_handle的唯一性
    pthread_mutex_lock(&cq_refcnt_with_lock.mutex);
    pid_t process_id=getpid();

    cq->handle=((uint32_t)process_id << 16)|cq_refcnt_with_lock.cq_refcnt;

    cq_refcnt_with_lock.cq_refcnt++;
    pthread_mutex_unlock(&cq_refcnt_with_lock.mutex);
    return cq;
}

struct ibv_qp *ibv_create_qp(struct ibv_pd *pd, struct ibv_qp_init_attr *qp_init_attr){
    if(qp_init_attr->recv_cq->handle!=qp_init_attr->send_cq->handle){
        LOG_ERROR_PRINTF("In current version, SQ and RQ must correspond to the same CQ\n");
        return NULL;
    }
    struct scale_qp* s_qp;
    s_qp = calloc(1, sizeof(struct scale_qp));
    struct ibv_qp* qp;
    qp=&(s_qp->qp);
    qp->context    	     = pd->context;
	qp->qp_context 	     = qp_init_attr->qp_context;
	qp->pd         	     = pd;
	qp->send_cq    	     = qp_init_attr->send_cq;
	qp->recv_cq    	     = qp_init_attr->recv_cq;
	qp->srq        	     = qp_init_attr->srq;
	qp->qp_type          = qp_init_attr->qp_type;
	qp->state	     = IBV_QPS_RESET;
	qp->events_completed = 0;

	pthread_mutex_init(&qp->mutex, NULL);
	pthread_cond_init(&qp->cond, NULL);
    pthread_mutex_lock(&qp_refcnt_with_lock.mutex);
    pid_t process_id=getpid();
    qp->handle=((uint32_t)process_id<<16)|qp_refcnt_with_lock.qp_refcnt;
    qp_refcnt_with_lock.qp_refcnt++;
    pthread_mutex_unlock(&qp_refcnt_with_lock.mutex);

    uint16_t low_word = qp->handle & 0xFFFF;
    cq_qp_map[qp_init_attr->send_cq->handle & 0xFFFF] = low_word;

    struct IBV_CREATE_QP_REQ req;
    req.send_cq_handle=qp_init_attr->send_cq->handle;
    req.recv_cq_handle=qp_init_attr->recv_cq->handle;

    req.qp_handle=qp->handle;
    struct IBV_CREATE_QP_RSP rsp;
    int rsp_size=sizeof(struct IBV_CREATE_QP_RSP);
    int rc = request_router(IBV_CREATE_QP, &req, (void*)&rsp, &rsp_size);
    if(rc==-1){
        LOG_ERROR_PRINTF("Fail to request router when create qp\n");
        free(qp);
        return NULL;
    }

    qp->qp_num=rsp.qp_num;
    return qp;

}

int ibv_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,int attr_mask){
    if (attr_mask & IBV_QP_STATE){
        struct IBV_MODIFY_QP_REQ req;
        struct IBV_MODIFY_QP_RSP rsp;
        int rsp_size=sizeof(struct IBV_MODIFY_QP_RSP);
        switch (attr->qp_state)
        {
            case IBV_QPS_INIT:{
                qp->state = attr->qp_state;
            }
            break;
            case IBV_QPS_RTR:{
                //todo: 检查attr输入是否为空
                struct scale_qp* s_qp=  container_of(qp, struct scale_qp, qp);
                req.dst_lid= attr->ah_attr.dlid;
                req.dst_v_qp_num = attr->dest_qp_num;
                req.qp_state=attr->qp_state;
                req.src_v_qp_handle=qp->handle;
                req.src_v_qp_num=qp->qp_num;
                s_qp->dst_v_qp_num = attr->dest_qp_num;
                memcpy(req.dst_gid,&(attr->ah_attr.grh.dgid),16);

                int rc=request_router(IBV_MODIFY_QP,&req,(void*)&rsp,&rsp_size);
                if(rc==-1){
                    LOG_ERROR_PRINTF("Fail to requset router when modify qp to RTR\n");
                    return -1;
                }
                qp->state = attr->qp_state;
            }
            break;
            case IBV_QPS_RTS:{
                qp->state = attr->qp_state;
            }
            break;

            default:{
                LOG_ERROR_PRINTF("Router dosen't allow user to modify QP state other than INIT, RTR and RTS\n");
            }
            break;
        }

		
    }

    //router没有开放给用户设置QP属性的权限，因此除了IBV_QP_STATE以外的大部分参数都被直接忽略了
    return 0;
}


int ibv_destroy_cq(struct ibv_cq *cq){
    struct IBV_DESTROY_CQ_REQ req;
    req.cq_handle = cq->handle;
    struct IBV_DESTROY_CQ_RSP rsp;
    int rsp_size = sizeof(struct IBV_DESTROY_CQ_RSP);
    //int rc = request_router(IBV_DESTROY_CQ, &req, (void*)&rsp, &rsp_size);
    int rc =1;
    rsp.flags=1;
    if(rc==-1){
        LOG_ERROR_PRINTF("Fail to request router when destroy cq");
        return -1;
    }
    if(rsp.flags){
        free(cq);
        cq=NULL;
        return 0;
    }
    else{
        return -1;
    }
}

int ibv_destroy_qp(struct ibv_qp *qp){
    struct scale_qp* s_qp = container_of(qp, struct scale_qp, qp);
    struct IBV_DESTROY_QP_REQ req;
    req.qp_handle=qp->handle;
    req.dst_v_qp_num =s_qp->dst_v_qp_num;
    struct IBV_DESTROY_QP_RSP rsp;
    int rsp_size = sizeof(struct IBV_DESTROY_QP_RSP);
    int rc = request_router(IBV_DESTROY_QP, &req, (void*)&rsp, &rsp_size);
    if(rc==-1){
        LOG_ERROR_PRINTF("Fail to request router when destroy qp");
        return -1;
    }
    if(rsp.flags){
        free(qp);
        qp=NULL;
        return 0;
    }
    else{
        return -1;
    }
}

int ibv_dereg_mr(struct ibv_mr *mr){
    struct IBV_DEREG_MR_REQ req;
    req.length=mr->length;
    req.lkey = mr->lkey;
    struct scale_mr* s_mr = container_of(mr,struct scale_mr, mr);
    strcpy(req.shm_name, s_mr->shm_name);
    req.router_addr = s_mr->router_addr;
    req.router_mr = s_mr->router_mr;
    req.router_shm_fd = s_mr->router_shm_fd;
    LOG_TRACE_PRINTF("The shm_name is %s\n", s_mr->shm_name);
    struct IBV_DEREG_MR_RSP rsp;
    int rsp_size = sizeof(struct IBV_DEREG_MR_RSP);
    int rc = request_router(IBV_DEREG_MR, &req, (void*)&rsp, &rsp_size);
    if(rc==-1){
        LOG_ERROR_PRINTF("Fail to request router when dereg mr\n");
        return -1;
    }
    if(rsp.flags){
        free(mr);
        mr=NULL;
        return 0;
    }
    else{
        return -1;
    }
}

int ibv_dealloc_pd(struct ibv_pd *pd){
    free(pd);
    return 0;
}

int ibv_close_device(struct ibv_context *context){
    free(context);
    return 0;
}

void ibv_free_device_list(struct ibv_device **list){
    free(*list);
    free(list);
}



#ifndef SCALE_TYPE_H
#define SCALE_TYPE_H
#define MAX_HOST_NUM 10240
#define MAP_SIZE MAX_HOST_NUM

// use page as deafult mem unit
#define PAGE_SIZE 4096 
#define AGENT_DOMAIN "/tmp/scale.domain"

typedef enum SCALE_FUNCTION_CALL{
    INIT_RES,
    // after exchange meta nic via socket
    CREATE_RES,
    REG_MR,

    // after exchange meta conn via socket
    SETUP_CONN,

    POST_SEND,
    POST_RECV,
    POLL_CQ,

    DEREG_MR,
    DESTROY_RES,
    //additional function call which support Verbs API
    IBV_GET_DEVICE_LIST,
    IBV_ALLOC_PD,
    IBV_REG_MR,
    IBV_QUERY_GID,
    IBV_QUERY_PORT,
    IBV_CREATE_QP,
    IBV_MODIFY_QP,
    IBV_POST_SEND,
    IBV_POST_RECV,
    IBV_POLL_CQ,
    IBV_DESTROY_CQ,
    IBV_DESTROY_QP,
    IBV_DEREG_MR
} SCALE_FUNCTION_CALL;

struct ScaleReqHeader
{
    pid_t process_id;
    SCALE_FUNCTION_CALL func;
    uint32_t body_size;
};
struct ScaleRspHeader
{
    uint32_t body_size;
};


struct IBV_GET_DEVICE_LIST_RSP{
    uint32_t node_type;
    uint32_t transport_type;
    /* Name of underlying kernel IB device, eg "mthca0" */
    char			name[64];
	/* Name of uverbs device, eg "uverbs0" */
	char			dev_name[64];
	/* Path to infiniband_verbs class device in sysfs */
	char			dev_path[256];
	/* Path to infiniband class device in sysfs */
	char			ibdev_path[256];
};

struct IBV_ALLOC_PD_RSP
{
    uint32_t handle;
};

struct IBV_REG_MR_REQ
{
    uint32_t access_flag;
    size_t length;
    void * client_addr;
    
    //uint32_t pd_handle;
};

struct IBV_REG_MR_RSP{
    uint32_t handle;
    uint32_t lkey;
    uint32_t rkey;
    char shm_name[100];
    void* router_mr;
    void* router_addr;
    int router_shm_fd;
};

struct IBV_QUERY_GID_RSP{
    uint8_t gid[16]; 
};

struct IBV_QUERY_PORT_RSP{
    uint32_t        state;
    uint32_t		max_mtu;
	uint32_t		active_mtu;
	int			    gid_tbl_len;
	uint32_t		port_cap_flags;
	uint32_t		max_msg_sz;
	uint32_t		bad_pkey_cntr;
	uint32_t		qkey_viol_cntr;
	uint16_t		pkey_tbl_len;
	uint16_t		lid;
	uint16_t		sm_lid;
	uint8_t			lmc;
	uint8_t			max_vl_num;
	uint8_t			sm_sl;
	uint8_t			subnet_timeout;
	uint8_t			init_type_reply;
	uint8_t			active_width;
	uint8_t			active_speed;
	uint8_t			phys_state;
	uint8_t			link_layer;
};

struct IBV_CREATE_QP_REQ
{
  uint32_t send_cq_handle;
  uint32_t recv_cq_handle;
  uint32_t qp_handle;  
};

struct IBV_CREATE_QP_RSP{
  uint32_t qp_num;
};

struct IBV_MODIFY_QP_REQ
{
    uint8_t dst_gid[16];
    uint16_t dst_lid;
    uint32_t dst_v_qp_num;
    uint32_t src_v_qp_num;
    uint32_t src_v_qp_handle;
    uint32_t qp_state;
};

struct IBV_MODIFY_QP_RSP
{
    uint8_t flag;
};

struct IBV_POST_SEND_REQ{
    uint32_t qp_handle;
    void* client_addr;
    int opcode;
    int wr_id;
    uint32_t lkey;
    size_t length;
    void* remote_router_addr;
    uint32_t rkey;
};

struct IBV_POST_SEND_RSP{
    int wr_id;
};

struct IBV_POST_RECV_REQ{
    uint32_t qp_handle;
    void* client_addr;
    uint32_t lkey;
    int wr_id;
    size_t length;

};

struct IBV_POST_RECV_RSP{
    int wr_id;
};

struct IBV_POLL_CQ_REQ{
    uint32_t cq_handle;
    uint32_t qp_handle;
    int num_entries;
};

struct IBV_POLL_CQ_RSP{
    int count;
    uint64_t wr_id;
    int opcode;
    int status;
    uint32_t		vendor_err;
};

struct IBV_DESTROY_CQ_REQ{
    uint32_t cq_handle;
};

struct IBV_DESTROY_CQ_RSP{
    uint8_t flags;
};

struct IBV_DESTROY_QP_REQ{
    uint32_t qp_handle;
    uint32_t dst_v_qp_num;
};

struct IBV_DESTROY_QP_RSP{
    uint8_t flags;
};


struct IBV_DEREG_MR_REQ{
    char shm_name[100];
    uint32_t lkey;
    int router_shm_fd;
    size_t length;
    void* router_mr;
    void* router_addr;
};

struct IBV_DEREG_MR_RSP{
    uint8_t flags;
};
#endif
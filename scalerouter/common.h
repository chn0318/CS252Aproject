/** common data structure client&router */

// max number of hosts
#define MAX_HOST_NUM 10240
#define MAP_SIZE MAX_HOST_NUM

// use page as deafult mem unit
#define PAGE_SIZE 4096 

// define default unix communicaiton domain
#define AGENT_DOMAIN "/tmp/scale.domain"

/* key of src---dst host (PF/VF) */
struct HostKey{
    ibv_gid src_gid;     /* RNIC src gid */
    ibv_gid dst_gid;     /* RNIC dst gid */

    bool operator==(const HostKey tmp) const {
        int src = memcmp(&src_gid, &tmp.src_gid, 16);
        int dst =  memcmp(&dst_gid, &tmp.dst_gid, 16);
        if (src == 0 && dst == 0)
            return true;
        else
            return false;
    }
}__attribute__((packed));

namespace std {
    template <>
    struct hash<HostKey>
    {
        std::size_t operator()(const HostKey& tmp) const
        {
            using std::hash;
            using std::string;

            // Compute individual hash values for each item, combine them using XOR and bit shifting:
            std::string src = reinterpret_cast<char *>((uint8_t *)&tmp.src_gid);
            std::string dst = reinterpret_cast<char *>((uint8_t *)&tmp.dst_gid);
            return hash<string>()(src) ^ (hash<string>()(dst) << 1);
        }
    };
}

/* local LID of IB port in src---dst host (PF/VF), maybe not considered in RoCEv2 */
struct HostLid{
    uint16_t src_lid;	/* LID of the IB port, src or local */
    uint16_t dst_lid;   /* LID of the IB prot, dst or remote, used for build QP connection */
}__attribute__((packed));

struct ConnKey{
    // uint8_t src_gid[16]; /* RNIC src gid */
    // uint8_t dst_gid[16]; /* RNIC dst gid */
    // ibv_gid src_gid;     /* RNIC src gid */
    // ibv_gid dst_gid;     /* RNIC dst gid */
    struct HostKey host_key; /* RNIC src---dst gid */
    uint32_t src_qpn;        /* RNIC src qpn */
    uint32_t dst_qpn;        /* RNIC dst qpn */
}__attribute__((packed));

struct ConnId{
    struct ConnKey conn_key; /* identifier of QP connection */
    pid_t process_id;        /* current process id */

    bool operator==(const ConnId tmp) const {
        if (process_id == tmp.process_id && conn_key.host_key == tmp.conn_key.host_key && conn_key.src_qpn ==  tmp.conn_key.src_qpn && conn_key.dst_qpn == tmp.conn_key.dst_qpn){
            return true;
        }
        else{
            return false;
        }
    }
}__attribute__((packed));

namespace std {
    template <>
    struct hash<ConnId>
    {
        std::size_t operator()(const ConnId& tmp) const
        {
            using std::hash;
            using std::string;

            // Compute individual hash values for each item, combine them using XOR and bit shifting:
            return (((hash<int>()(tmp.process_id) ^ 
                        (hash<HostKey>()(tmp.conn_key.host_key) << 1)) >> 1) 
                        ^ (hash<int>()(tmp.conn_key.src_qpn) << 1) >> 1) 
                        ^ (hash<int>()(tmp.conn_key.dst_qpn) << 1);
        }
    };
}

/* struct to fill conn_id and local/remote lid */
struct ConnInfo{
    struct ConnId conn_id;
    struct HostLid host_lid;
}__attribute__((packed));

/* local memory item */
/* reg_memory (pointer & size), allocate as aligned page (client), export as shared memory (router), register at RNIC */
struct LocalMem{
    struct ConnId *conn_id;
    void *client_addr;      /* virutal address of local client */
    size_t			length; /* virtual address length of remote */  

	void *router_addr;      /* virtual address of local router */
    void *shm_piece;        /* pointer to shm_piece at  router */
    char shm_name[100];     /* shared mem name of local router */
	int shm_fd;             /* shared mem fd   of local router */

    struct ibv_mr *mr;      /* pointer to MR, valid router only*/
	uint32_t		lkey;
	uint32_t		rkey;
}__attribute__((packed));

/* remote memory item */
struct RemoteMem{
    struct ConnId *conn_id;
    void *router_addr;         /* virtual address of remote router */ 
    size_t          length;    /* virtual address length of remote */
	uint32_t		rkey;
}__attribute__((packed));

typedef enum SCALE_FUNCTION_CALL{
    INIT_RES,
    CREATE_RES,
    REG_MR,

    // after exchange meta data via socket
    SETUP_CONN,

    POST_SEND,
    POST_RECV,
    POLL_CQ,

    DEREG_MR,
    DESTROY_RES,

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

/* no request body is required for INIT_RES */
// struct INIT_RES_REQ{
// };

struct INIT_RES_RSP{
    struct ConnInfo conn_info;
};

struct CREATE_RES_REQ{
    struct HostKey host_key;
};

struct CREATE_RES_RSP{
    struct ConnId conn_id;
};

struct REG_MR_REQ{
    struct ConnId conn_id;
    struct LocalMem local_mem;
};

struct REG_MR_RSP{
    struct LocalMem local_mem;
};

struct SETUP_CONN_REQ{
    struct ConnInfo conn_info;
};

struct SETUP_CONN_RSP{
    pid_t process_id;
    std::atomic<uint8_t> status; 
};

struct POST_SEND_REQ{
    struct ConnId conn_id;

    int wr_id;
    void *local_addr;
    size_t length;
    uint32_t lkey;

    int opcode;
    void *remote_addr;
    uint32_t rkey;
};

struct POST_SEND_RSP{
    int wr_id;
};

struct POST_RECV_REQ{
    struct ConnId conn_id;

    int wr_id;
    void *local_addr;
    size_t length;
    uint32_t lkey;
};

struct POST_RECV_RSP{
    int wr_id;
};

struct POLL_CQ_REQ{
    struct ConnId conn_id;
    int wr_id;
    int count;
};

struct POLL_CQ_RSP{
    int wr_id;
    int count;
};

struct DEREG_MR_REQ{
    struct ConnId conn_id;
    struct LocalMem local_mem;
};

struct DEREG_MR_RSP{
    struct LocalMem local_mem;
};

struct DESTROY_RES_REQ{
    struct ConnId conn_id;
};

struct DESTROY_RES_RSP{
    struct ConnId conn_id;
};

struct IBV_GET_DEVICE_LIST_RSP{
    uint32_t node_type;
    uint32_t transport_type;
    /* Name of underlying kernel IB device, eg "mthca0" */
    char			name[IBV_SYSFS_NAME_MAX];
	/* Name of uverbs device, eg "uverbs0" */
	char			dev_name[IBV_SYSFS_NAME_MAX];
	/* Path to infiniband_verbs class device in sysfs */
	char			dev_path[IBV_SYSFS_PATH_MAX];
	/* Path to infiniband class device in sysfs */
	char			ibdev_path[IBV_SYSFS_PATH_MAX];
};

struct IBV_ALLOC_PD_RSP{
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
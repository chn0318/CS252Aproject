#include"scale.h"


void init_unix_sock(){

}

struct sock_with_lock* get_unix_sock(SCALE_FUNCTION_CALL req)
{
    struct sock_with_lock* unix_sock = (struct sock_with_lock*)malloc(sizeof(struct sock_with_lock));
    unix_sock->sock = -1;

	if (unix_sock->sock < 0)
	{
		connect_router(unix_sock);
	}

	return unix_sock;
}

void connect_router(struct sock_with_lock *unix_sock)
{
    register int len;
    struct sockaddr_un srv_addr;

    // pthread_mutex_lock(&(unix_sock->mutex));

    if ((unix_sock->sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        LOG_ERROR_PRINTF("client: socket");
        exit(1);
    }
    
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_UNIX;
	strcpy(srv_addr.sun_path, AGENT_DOMAIN);
    if (connect(unix_sock->sock, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
        LOG_ERROR_PRINTF("client: cannot connect server domain");
        unix_sock->sock = -1;
    }

    // pthread_mutex_unlock(&(unix_sock->mutex));
}

int request_router(SCALE_FUNCTION_CALL req, void* req_body, void *rsp, int *rsp_size)
{
	// /* for cq event */
	// int mapped_fd = -1;

	/* return code of request result */
	int rc = 0;
	struct sock_with_lock* unix_sock = NULL;

    unix_sock = get_unix_sock(req);

	if (unix_sock == NULL || unix_sock->sock < 0)
	{
		LOG_ERROR_PRINTF("invalid socket.");
		rc = -1;
		return rc;
	}

	struct ScaleReqHeader header;

	header.process_id = getpid();
	header.func = req;
	header.body_size = 0;
	int fixed_size_rsp = 1;
    int close_sock = 1;

	int bytes = 0;
	
	switch (req)
	{
		/*case INIT_RES:
			*rsp_size = sizeof(struct INIT_RES_RSP);
			header.body_size = 0;
			break;

		case CREATE_RES:
			*rsp_size = sizeof(struct CREATE_RES_RSP);
			header.body_size = sizeof(struct CREATE_RES_REQ);
			break;

		case REG_MR:
			*rsp_size = sizeof(struct REG_MR_RSP);
			header.body_size = sizeof(struct REG_MR_REQ);
			break;
		
		case SETUP_CONN:
			*rsp_size = sizeof(struct SETUP_CONN_RSP);
			header.body_size = sizeof(struct SETUP_CONN_REQ);
			break;
		
		case POST_SEND:
			*rsp_size = sizeof(struct POST_SEND_RSP);
			header.body_size = sizeof(struct POST_SEND_REQ);
			break;

		case POST_RECV:
			*rsp_size = sizeof(struct POST_RECV_RSP);
			header.body_size = sizeof(struct POST_RECV_REQ);
			break;

		case POLL_CQ:
			// fixed_size_rsp = 0;
			*rsp_size = sizeof(struct POLL_CQ_RSP);
			header.body_size = sizeof(struct POLL_CQ_REQ);
			break;
		
		case DEREG_MR:
			*rsp_size = sizeof(struct DEREG_MR_RSP);
			header.body_size = sizeof(struct DEREG_MR_REQ);
			break;

		case DESTROY_RES:
			*rsp_size = sizeof(struct DESTROY_RES_RSP);
			header.body_size = sizeof(struct DESTROY_RES_REQ);
			break;
        */
        //additional funcation call which support Verbs API
        case IBV_GET_DEVICE_LIST:
            *rsp_size=sizeof(struct IBV_GET_DEVICE_LIST_RSP);
            header.body_size = 0;
            break;
		case IBV_ALLOC_PD:
			*rsp_size=sizeof(struct IBV_ALLOC_PD_RSP);
			header.body_size = 0;
			break;
		case IBV_REG_MR:
			*rsp_size=sizeof(struct IBV_REG_MR_RSP);
			header.body_size=sizeof(struct IBV_REG_MR_REQ);
			break;
		case IBV_QUERY_GID:
			*rsp_size=sizeof(struct IBV_QUERY_GID_RSP);
			header.body_size=0;
			break;
		case IBV_QUERY_PORT:
			*rsp_size=sizeof(struct IBV_QUERY_PORT_RSP);
			header.body_size=0;
			break;
		case IBV_CREATE_QP:
			*rsp_size=sizeof(struct IBV_CREATE_QP_RSP);
			header.body_size=sizeof(struct IBV_CREATE_QP_REQ);
			break;
		case IBV_MODIFY_QP:
			*rsp_size=sizeof(struct IBV_MODIFY_QP_RSP);
			header.body_size=sizeof(struct IBV_MODIFY_QP_REQ);
			break;
		case IBV_POST_SEND:
			*rsp_size=sizeof(struct IBV_POST_SEND_RSP);
			header.body_size=sizeof(struct IBV_POST_SEND_REQ);
			break;
		case IBV_POST_RECV:
			*rsp_size=sizeof(struct IBV_POST_RECV_RSP);
			header.body_size=sizeof(struct IBV_POST_RECV_REQ);
			break;
		case IBV_POLL_CQ:
			*rsp_size = sizeof(struct IBV_POLL_CQ_RSP);
			header.body_size = sizeof(struct IBV_POLL_CQ_REQ);
			break;
		case IBV_DESTROY_CQ:
			*rsp_size = sizeof(struct IBV_DESTROY_CQ_RSP);
			header.body_size = sizeof(struct IBV_DESTROY_CQ_REQ);
			break;
		case IBV_DESTROY_QP:
			*rsp_size = sizeof(struct IBV_DESTROY_QP_RSP);
			header.body_size = sizeof(struct IBV_DESTROY_QP_REQ);
			break;
		case IBV_DEREG_MR:
			*rsp_size = sizeof(struct IBV_DEREG_MR_RSP);
			header.body_size = sizeof(struct IBV_DEREG_MR_REQ);
			break;
		default:
			rc = -1;
			goto end;
	}

	// if (!close_sock)
	// {
	// 	pthread_mutex_lock(&(unix_sock->mutex));
	// }

	int n;
	if ((n = write(unix_sock->sock, &header, sizeof(header))) < sizeof(header))
	{
		if (n < 0)
		{
			LOG_ERROR_PRINTF("router disconnected in writing req header.\n");
			fflush(stdout);
			unix_sock->sock = -1;
		}
		else
		{
			LOG_TRACE_PRINTF("partial write\n");
		}

		rc = -1;
		goto end;
	}

	if (header.body_size > 0)
	{
		if ((n = write(unix_sock->sock, req_body, header.body_size)) < header.body_size)
		{
			if (n < 0)
			{
				LOG_ERROR_PRINTF("router disconnected in writing req body.");
				fflush(stdout);
				unix_sock->sock = -1;
			}
			else
			{
				LOG_TRACE_PRINTF("partial write\n");
			}

			rc = -1;
			goto end;
		}	
	}

	/* for cq event */
	// if (req == IBV_CREATE_COMP_CHANNEL)
	// {
	// 	mapped_fd = recv_fd(unix_sock);
	// }

	if (!fixed_size_rsp)
	{
		struct ScaleRspHeader rsp_hr;
		bytes = 0;
		while(bytes < sizeof(rsp_hr)) {
			n = read(unix_sock->sock, ((char *)&rsp_hr) + bytes, sizeof(rsp_hr) - bytes);
			if (n < 0)
			{
				LOG_ERROR_PRINTF("router disconnected when reading rsp.");
				fflush(stdout);
				unix_sock->sock = -1;

				rc = -1;
				goto end;
			}
			bytes = bytes + n;
		}

		*rsp_size = rsp_hr.body_size;
	}

	bytes = 0;
	while(bytes < *rsp_size)
	{
		n = read(unix_sock->sock, (char*)rsp + bytes, *rsp_size - bytes);
		if (n < 0)
		{
			LOG_ERROR_PRINTF("router disconnected when reading rsp.");
			fflush(stdout);
			unix_sock->sock = -1;

			rc = -1;
			goto end;
		}

		bytes = bytes + n;
	}

	/** event ibv_get_cq_event is not implementated yet */
	// if (req == IBV_CREATE_COMP_CHANNEL)
	// {
	// 	printf("[INFO Comp Channel] rsp.ec.fd = %d, mapped_fd = %d\n", ((struct IBV_CREATE_COMP_CHANNEL_RSP*)rsp)->fd, mapped_fd);
	// 	fflush(stdout);

	// 	comp_channel_map[mapped_fd] = ((struct IBV_CREATE_COMP_CHANNEL_RSP*)rsp)->fd;
	// 	((struct IBV_CREATE_COMP_CHANNEL_RSP*)rsp)->fd = mapped_fd;
	// }

end:
    if (close_sock)
	{
		close(unix_sock->sock);
        // pthread_mutex_destroy(&unix_sock->mutex);
		free(unix_sock);
	}
	// else 
	// {
	// 	/* for cq event */
	// 	pthread_mutex_unlock(&(unix_sock->mutex)); 
	// }

	return rc;
}
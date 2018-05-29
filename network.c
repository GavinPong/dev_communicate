/****************下一步需要改善的地方
1、parse_ip_from_str使用一个独立的线程去执行，避免gethostbyname阻塞接口
2、多播协议还要进一步做实验，验证其属性
********************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include "network.h"


#ifdef LINUX_PLATFORM
#define Sleep(x) usleep((x) * 1000)
#endif

#define SOCK_ARRY_DEF 100		//套接字数组默认大小
#define SOCK_ARRY_STEP 50		//套接字数组增大步长
#define LISTEN_CNT 5			//listen接口队列大小

#define DST_IP_MAX 64			//目标ip数组大小

#define SOCKET_ERROR -1

#ifndef DBG_PRINTF
#define DBG_PRINTF printf
#endif

#define NET_HANDLE_2_SOCK_ID(m) (int )(m)  

typedef struct _net_param_s
{
	int m_sock;
	net_protocol_type_e m_sock_protocol;
	server_callback m_callback;
	pthread_mutex_t m_mutex;
}net_param_t;

typedef struct _network_ctx_s
{
	net_param_t *m_sock_param_arry_ptr;				//套接字数组指正
	char m_arry_status;					//数组是否被被申请状态
	int m_cur_arry_max;					//当前数组大小
}network_ctx_t, *pnetwork_ctx_t;

typedef struct _accept_ctx_s
{
	net_param_t m_net_param;
	addr_info_t m_addr_info;
}accept_ctx_t;

static network_ctx_t s_network_ctx;

static int parse_ip_from_str(char *src, char *dst_buf, int buf_size)
{
	if (!src || !dst_buf || !buf_size)
	{
		DBG_PRINTF("%s:param invalid\n", __FUNCTION__);
		return -1;
	}
	struct hostent *host = NULL;
	struct in_addr dst_addr;
	char str[64] = "";

	memset(dst_buf, 0, buf_size);
	if (1 == inet_pton(AF_INET, src, &dst_addr))
	{
		memcpy(dst_buf, src, buf_size);
		return 0;
	}
	host = gethostbyname(src);
	if (host)
	{
		memcpy(dst_buf, inet_ntop(host->h_addrtype, host->h_addr, str, sizeof(str)), buf_size);
		return 0;
	}

	return -1;
}

static int network_alloc(network_ctx_t *network_ctx_ptr, int *available_sock_id)
{
	if (!network_ctx_ptr)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int i = 0;

	if (0 == s_network_ctx.m_arry_status)
	{
		s_network_ctx.m_arry_status = 1;
		s_network_ctx.m_sock_param_arry_ptr = (net_param_t *)calloc(SOCK_ARRY_DEF, sizeof(net_param_t));
		if (!s_network_ctx.m_sock_param_arry_ptr)
		{
			return NETWORK_ERR_NOMEMORY;
		}
		s_network_ctx.m_cur_arry_max = SOCK_ARRY_DEF;
	}
	for (; i < s_network_ctx.m_cur_arry_max;i++)
	{
		if (0 == s_network_ctx.m_sock_param_arry_ptr[i].m_sock)
		{
			*available_sock_id = i;
			break;
		}
	}
	if ((i >= s_network_ctx.m_cur_arry_max) && (0 != s_network_ctx.m_arry_status))
	{
		char *new_ptr = (char *)realloc(s_network_ctx.m_sock_param_arry_ptr, (s_network_ctx.m_cur_arry_max + SOCK_ARRY_STEP) * sizeof(net_param_t));
		if (!new_ptr)
		{
			return NETWORK_ERR_NOMEMORY;
		}
		//清空新增内存
		memset(new_ptr + s_network_ctx.m_cur_arry_max, 0, SOCK_ARRY_STEP * sizeof(int));
		*available_sock_id = s_network_ctx.m_cur_arry_max;
		s_network_ctx.m_cur_arry_max += SOCK_ARRY_STEP;
	}
	return NETWORK_OK;
}

void *udp_server_process(void *param)
{
	if (!param)
	{
		return NULL;
	}
	net_param_t *p_net_param = (net_param_t *)param;
	addr_info_t addr_info;

	addr_info.m_sock = p_net_param->m_sock;
	addr_info.m_data_len = 0;

	if (p_net_param->m_callback)
	{
		p_net_param->m_callback(&addr_info);
	}
	free(p_net_param);
}

void *broadcast_server_process(void *param)
{
	if (!param)
	{
		return NULL;
	}
	net_param_t *p_net_param = (net_param_t *)param;
	addr_info_t addr_info;

	addr_info.m_sock = p_net_param->m_sock;
	addr_info.m_data_len = 0;

	if (p_net_param->m_callback)
	{
		p_net_param->m_callback(&addr_info);
	}
	free(p_net_param);
}

void *multicast_server_process(void *param)
{
	if (!param)
	{
		return NULL;
	}
	net_param_t *p_net_param = (net_param_t *)param;
	addr_info_t addr_info;

	addr_info.m_sock = p_net_param->m_sock;
	addr_info.m_data_len = 0;

	if (p_net_param->m_callback)
	{
		p_net_param->m_callback(&addr_info);
	}
	free(p_net_param);
}

void *tcp_server_process(void *param)
{
	if (!param)
	{
		return NULL;
	}
	accept_ctx_t *p_accept_ctx = (accept_ctx_t *)param;
	p_accept_ctx->m_addr_info;
	if (p_accept_ctx->m_net_param.m_callback)
	{
		//DBG_PRINTF("%d:callback\n", __LINE__);
		p_accept_ctx->m_net_param.m_callback(&p_accept_ctx->m_addr_info);
	}
	else
	{
		//DBG_PRINTF("%d: exit callback\n", __LINE__);
	}
	free(p_accept_ctx);

	return NULL;
}

void *server_accpet(void *param)
{
	int client_sock = -1;
	struct sockaddr_in client_addr;
	int client_addr_len = 0;
	accept_ctx_t *p_accept_ctx = (accept_ctx_t *)param;
	int sockfd = p_accept_ctx->m_net_param.m_sock;
	pthread_t thread;

	while(1)
	{
		memset(&client_addr, 0, sizeof(client_addr));
		client_sock = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
		if (client_sock < 0)
		{
			break;
		}
		p_accept_ctx->m_net_param.m_sock = client_sock;
		p_accept_ctx->m_addr_info.m_sock =client_sock;
		p_accept_ctx->m_addr_info.m_data_len = client_addr_len;
		memcpy(p_accept_ctx->m_addr_info.data, &client_addr, sizeof(client_addr));
		if (!pthread_create(&thread, NULL, tcp_server_process, (void *)p_accept_ctx))
		{
			pthread_detach(thread);
		}
		else
		{
			close(client_sock);
			free(p_accept_ctx);
			return NULL;
		}
	}
	return NULL;
}

static int open_tcp(net_info_t net_info)
{
	struct sockaddr_in server_addr;
	char dst_ip[DST_IP_MAX] = "";
	int sockfd = -1;

	if(parse_ip_from_str(net_info.m_dev_addr, dst_ip, sizeof(dst_ip)) < 0)
		return NETWORK_ERR_DEV_ADDR;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = net_info.m_family;
	server_addr.sin_port = htons(net_info.m_dev_port);
	inet_pton(server_addr.sin_family, dst_ip, &server_addr.sin_addr);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		return NETWORK_ERR_SOCKED;
	}
	if (net_info.m_timeout)
	{
		struct timeval timeo;
		socklen_t len = sizeof(struct timeval);

		timeo.tv_sec = net_info.m_timeout;
		timeo.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	}
	if (net_info.m_recvbuf_size)
	{
		int recvbuf_size = net_info.m_recvbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&recvbuf_size, sizeof(int));
	}
	if (net_info.m_sendbuf_size)
	{
		int sendbuf_size = net_info.m_sendbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sendbuf_size, sizeof(int));
	}
	if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
	{
		close(sockfd);
		return NETWORK_ERR_CONNECT;
	}

	return sockfd;
}

static int open_udp(net_info_t net_info)
{
	struct sockaddr_in server_addr;
	char dst_ip[DST_IP_MAX] = "";
	int sockfd = -1;

	if(parse_ip_from_str(net_info.m_dev_addr, dst_ip, sizeof(dst_ip)) < 0)
		return NETWORK_ERR_DEV_ADDR;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = net_info.m_family;
	server_addr.sin_port = htons(net_info.m_dev_port);
	inet_pton(server_addr.sin_family, dst_ip, &server_addr.sin_addr);
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		return NETWORK_ERR_SOCKED;
	}
	if (net_info.m_timeout)
	{
		struct timeval timeo;
		socklen_t len = sizeof(struct timeval);

		timeo.tv_sec = net_info.m_timeout;
		timeo.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	}
	if (net_info.m_recvbuf_size)
	{
		int recvbuf_size = net_info.m_recvbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&recvbuf_size, sizeof(int));
	}
	if (net_info.m_sendbuf_size)
	{
		int sendbuf_size = net_info.m_sendbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sendbuf_size, sizeof(int));
	}
	//使用connet绑定sockfd和目标地址，以便直接使用tcp接口去操作，保证接口的简单性，和程序的稳定性
	if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
	{
		close(sockfd);
		return NETWORK_ERR_CONNECT;
	}
	return sockfd;
}

static int open_broadcast(net_info_t net_info)
{
	int sockfd = -1;
	struct sockaddr_in bc_addr;
	char dst_ip[DST_IP_MAX] = "";
	int so_broadcast = 1;
	int ret = -1;

	if(parse_ip_from_str(net_info.m_dev_addr, dst_ip, sizeof(dst_ip)) < 0)
		return NETWORK_ERR_DEV_ADDR;
	memset(&bc_addr, 0, sizeof(bc_addr));
	bc_addr.sin_family = net_info.m_family;
	bc_addr.sin_port = htons(net_info.m_dev_port);
	inet_pton(bc_addr.sin_family, dst_ip, &bc_addr.sin_addr);
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		return NETWORK_ERR_SOCKED;
	}
	setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &so_broadcast, sizeof(so_broadcast));
	//使用connet绑定sockfd和目标地址，以便直接使用tcp接口去操作，保证接口的简单性，和程序的稳定性
	if(connect(sockfd, (struct sockaddr *)&bc_addr, sizeof(bc_addr)))
	{
		close(sockfd);
		return NETWORK_ERR_CONNECT;
	}

	return sockfd;
}

static int open_multicast(net_info_t net_info)
{
	int sockfd = -1;
	char dst_ip[DST_IP_MAX] = "";
	struct sockaddr_in multi_addr;

	if(parse_ip_from_str(net_info.m_dev_addr, dst_ip, sizeof(dst_ip)) < 0)
		return NETWORK_ERR_DEV_ADDR;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		return NETWORK_ERR_SOCKED;
	}
	memset(&multi_addr, 0, sizeof(multi_addr));
	multi_addr.sin_family = net_info.m_family;
	multi_addr.sin_port = htons(net_info.m_dev_port);
	inet_pton(multi_addr.sin_family, dst_ip, &multi_addr.sin_addr);
	if(connect(sockfd, (struct sockaddr *)&multi_addr, sizeof(multi_addr)))
	{
		close(sockfd);
		return NETWORK_ERR_CONNECT;
	}

	return sockfd;
}

static int close_tcp(int sockid)
{
	if (sockid > s_network_ctx.m_cur_arry_max)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock = 0;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock_protocol = NET_PROTOCOL_TYPE_NULL;
	shutdown(sockfd, 2);
	close(sockfd);
	return NETWORK_OK;
}

static int close_udp(int sockid)
{
	if (sockid > s_network_ctx.m_cur_arry_max)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock = 0;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock_protocol = NET_PROTOCOL_TYPE_NULL;
	shutdown(sockfd, 2);
	close(sockfd);

	return NETWORK_OK;
}

static int close_broadcast(int sockid)
{
	if (sockid > s_network_ctx.m_cur_arry_max)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock = 0;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock_protocol = NET_PROTOCOL_TYPE_NULL;
	shutdown(sockfd, 2);
	close(sockfd);

	return NETWORK_OK;
}

static int close_multicast(int sockid)
{
	if (sockid > s_network_ctx.m_cur_arry_max)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock = 0;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock_protocol = NET_PROTOCOL_TYPE_NULL;
	shutdown(sockfd, 2);
	close(sockfd);

	return NETWORK_OK;
}

static int open_tcp_server(net_info_t net_info, server_callback callback)
{
	struct sockaddr_in server_addr;
	char dst_ip[DST_IP_MAX] = "";
	int sockfd = -1;
	pthread_t thread;
	accept_ctx_t *p_accept_ctx = NULL;

	if(parse_ip_from_str(net_info.m_dev_addr, dst_ip, sizeof(dst_ip)) < 0)
		return NETWORK_ERR_DEV_ADDR;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = net_info.m_family;
	server_addr.sin_port = htons(net_info.m_dev_port);
	inet_pton(server_addr.sin_family, dst_ip, &server_addr.sin_addr);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		return NETWORK_ERR_SOCKED;
	}
	if(bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
	{
		close(sockfd);
		return NETWORK_ERR_BIND;
	}
	if (net_info.m_timeout)
	{
		struct timeval timeo;
		socklen_t len = sizeof(struct timeval);

		timeo.tv_sec = net_info.m_timeout;
		timeo.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	}
	if (net_info.m_recvbuf_size)
	{
		int recvbuf_size = net_info.m_recvbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&recvbuf_size, sizeof(int));
	}
	if (net_info.m_sendbuf_size)
	{
		int sendbuf_size = net_info.m_sendbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sendbuf_size, sizeof(int));
	}
	listen(sockfd, LISTEN_CNT);
	p_accept_ctx = (accept_ctx_t *)calloc(1, sizeof(accept_ctx_t) + sizeof(struct sockaddr_in));
	if (!p_accept_ctx)
	{
		close(sockfd);
		return NETWORK_ERR_NOMEMORY;
	}
	p_accept_ctx->m_net_param.m_sock = sockfd;
	p_accept_ctx->m_net_param.m_callback = callback;
	p_accept_ctx->m_net_param.m_sock_protocol = net_info.m_pro_type;
	if (!pthread_create(&thread, NULL, server_accpet, (void *)p_accept_ctx))
	{
		pthread_detach(thread);
	}
	else
	{
		close(sockfd);
		free(p_accept_ctx);
		return NETWORK_ERR_THREAD;
	}

	return sockfd;
}

static int open_udp_server(net_info_t net_info, server_callback callback)
{
	struct sockaddr_in server_addr;
	char dst_ip[DST_IP_MAX] = "";
	int sockfd = -1;
	pthread_t thread;
	net_param_t *p_net_param = (net_param_t *)calloc(1, sizeof(net_param_t));

	if (!p_net_param)
	{
		return NETWORK_ERR_NOMEMORY;
	}

	if(parse_ip_from_str(net_info.m_dev_addr, dst_ip, sizeof(dst_ip)) < 0)
		return NETWORK_ERR_DEV_ADDR;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = net_info.m_family;
	server_addr.sin_port = htons(net_info.m_dev_port);
	inet_pton(server_addr.sin_family, dst_ip, &server_addr.sin_addr);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		return NETWORK_ERR_SOCKED;
	}
#if 0
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags >= 0)
	{
		fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	}
#endif
	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
	{
		close(sockfd);
		return NETWORK_ERR_BIND;
	}
	if (net_info.m_timeout)
	{
		struct timeval timeo;
		socklen_t len = sizeof(struct timeval);

		timeo.tv_sec = net_info.m_timeout;
		timeo.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	}
	if (net_info.m_recvbuf_size)
	{
		int recvbuf_size = net_info.m_recvbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&recvbuf_size, sizeof(int));
	}
	if (net_info.m_sendbuf_size)
	{
		int sendbuf_size = net_info.m_sendbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sendbuf_size, sizeof(int));
	}
	p_net_param->m_callback = callback;
	p_net_param->m_sock = sockfd;
	p_net_param->m_sock_protocol = net_info.m_pro_type;
	if (!pthread_create(&thread, NULL, udp_server_process, (void *)p_net_param))
	{
		pthread_detach(thread);
	}
	else
	{
		close(sockfd);
		free(p_net_param);
		return NETWORK_ERR_THREAD;
	}
	return sockfd;
}

static int open_broadcast_server(net_info_t net_info, server_callback callback)
{
	struct sockaddr_in server_addr;
	char dst_ip[DST_IP_MAX] = "";
	int sockfd = -1;
	pthread_t thread;
	net_param_t *p_net_param = (net_param_t *)calloc(1, sizeof(net_param_t));

	if (!p_net_param)
	{
		return NETWORK_ERR_NOMEMORY;
	}

	if(parse_ip_from_str(net_info.m_dev_addr, dst_ip, sizeof(dst_ip)) < 0)
		return NETWORK_ERR_DEV_ADDR;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = net_info.m_family;
	server_addr.sin_port = htons(net_info.m_dev_port);
	inet_pton(server_addr.sin_family, dst_ip, &server_addr.sin_addr);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		return NETWORK_ERR_SOCKED;
	}
#if 0
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags >= 0)
	{
		fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	}
#endif
	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
	{
		close(sockfd);
		return NETWORK_ERR_BIND;
	}
	if (net_info.m_timeout)
	{
		struct timeval timeo;
		socklen_t len = sizeof(struct timeval);

		timeo.tv_sec = net_info.m_timeout;
		timeo.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	}
	if (net_info.m_recvbuf_size)
	{
		int recvbuf_size = net_info.m_recvbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&recvbuf_size, sizeof(int));
	}
	if (net_info.m_sendbuf_size)
	{
		int sendbuf_size = net_info.m_sendbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sendbuf_size, sizeof(int));
	}
	p_net_param->m_callback = callback;
	p_net_param->m_sock = sockfd;
	p_net_param->m_sock_protocol = net_info.m_pro_type;
	if (!pthread_create(&thread, NULL, broadcast_server_process, (void *)p_net_param))
	{
		pthread_detach(thread);
	}
	else
	{
		close(sockfd);
		free(p_net_param);
		return NETWORK_ERR_THREAD;
	}
	return sockfd;
}

static int open_multicast_server(net_info_t net_info, server_callback callback)
{
	struct sockaddr_in server_addr;
	char dst_ip[DST_IP_MAX] = "";
	int sockfd = -1;
	int ret = -1;
	pthread_t thread;
	net_param_t *p_net_param = (net_param_t *)calloc(1, sizeof(net_param_t));

	if (!p_net_param)
	{
		return NETWORK_ERR_NOMEMORY;
	}

	if(parse_ip_from_str(net_info.m_dev_addr, dst_ip, sizeof(dst_ip)) < 0)
		return NETWORK_ERR_DEV_ADDR;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = net_info.m_family;
	server_addr.sin_port = htons(net_info.m_dev_port);
	inet_pton(server_addr.sin_family, dst_ip, &server_addr.sin_addr);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		return NETWORK_ERR_SOCKED;
	}
#if 0
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags >= 0)
	{
		fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	}
#endif
	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
	{
		close(sockfd);
		return NETWORK_ERR_BIND;
	}
	//add to multicast
	struct ip_mreq mreq;   
	memset(&mreq, 0, sizeof(mreq));  
	//本机地址   
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);   
	//多播地址 
	inet_pton(AF_INET, dst_ip, &mreq.imr_multiaddr); 
	//加多播组   
	ret = setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,(void *)&mreq, sizeof(mreq));   
	if (net_info.m_timeout)
	{
		struct timeval timeo;
		socklen_t len = sizeof(struct timeval);

		timeo.tv_sec = net_info.m_timeout;
		timeo.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	}
	if (net_info.m_recvbuf_size)
	{
		int recvbuf_size = net_info.m_recvbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&recvbuf_size, sizeof(int));
	}
	if (net_info.m_sendbuf_size)
	{
		int sendbuf_size = net_info.m_sendbuf_size;
		setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sendbuf_size, sizeof(int));
	}
	p_net_param->m_callback = callback;
	p_net_param->m_sock = sockfd;
	p_net_param->m_sock_protocol = net_info.m_pro_type;
	if (!pthread_create(&thread, NULL, multicast_server_process, (void *)p_net_param))
	{
		pthread_detach(thread);
	}
	else
	{
		close(sockfd);
		free(p_net_param);
		return NETWORK_ERR_THREAD;
	}
	return sockfd;
}

static int close_tcp_server(int sockid)
{
	if (sockid > s_network_ctx.m_cur_arry_max)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock = 0;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock_protocol = NET_PROTOCOL_TYPE_NULL;
	shutdown(sockfd, 2);
	close(sockfd);
	return NETWORK_OK;
}

static int close_udp_server(int sockid)
{
	if (sockid > s_network_ctx.m_cur_arry_max)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock = 0;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock_protocol = NET_PROTOCOL_TYPE_NULL;
	shutdown(sockfd, 2);
	close(sockfd);

	return NETWORK_OK;
}

static int close_broadcast_server(int sockid)
{
	if (sockid > s_network_ctx.m_cur_arry_max)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock = 0;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock_protocol = NET_PROTOCOL_TYPE_NULL;
	shutdown(sockfd, 2);
	close(sockfd);

	return NETWORK_OK;
}

static int close_multicast_server(int sockid)
{
	if (sockid > s_network_ctx.m_cur_arry_max)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int ret = -1;
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock;

	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock = 0;
	s_network_ctx.m_sock_param_arry_ptr[sockid].m_sock_protocol = NET_PROTOCOL_TYPE_NULL;
	shutdown(sockfd, 2);
	close(sockfd);

	return NETWORK_OK;
}

static int recv_tcp(int sockfd, char *buf, int buf_size)
{
	if (!buf || !buf_size)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	return recv(sockfd, buf, buf_size, 0);
// 	int ret = -1;
// 	int off = 0;
// 	int len1 = buf_size;
// 
// 	while(len1 > 0)
// 	{
// 		ret = recv(sockfd, buf + off, len1, 0);
// 		if (0 == ret || SOCKET_ERROR == ret)
// 		{
// 			return NETWORK_ERR_RECV;
// 		}
// 		else
// 		{
// 			len1 -= ret;
// 			off += ret;
// 			if (len1 > 0)
// 			{
// 				Sleep(1);
// 			}
// 		}
// 	}
// 	return NETWORK_OK;
}

static int recv_udp(int sockfd, char *buf, int buf_size)
{
	if (!buf || !buf_size)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	return recv_tcp(sockfd, buf, buf_size);
// 	int ret = -1;
// 	int off = 0;
// 	int len1 = buf_size;
// 	
// 	while(len1 > 0)
// 	{
// 		ret = recv(sockfd, buf + off, len1, 0);
// 		if (0 == ret || SOCKET_ERROR == ret)
// 		{
// 			return NETWORK_ERR_RECV;
// 		}
// 		else
// 		{
// 			len1 -= ret;
// 			off += ret;
// 			if (len1 > 0)
// 			{
// 				Sleep(1);
// 			}
// 		}
// 	}
// 
// 	return NETWORK_OK;
}

static int send_tcp(int sockfd, char *buf, int buf_size)
{
	if (!buf || !buf_size)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int off = 0;
	int ret = 0;

	while(buf_size > 0)
	{
		ret = send(sockfd, buf + off, buf_size, 0);
		if (0 == ret || SOCKET_ERROR == ret)
		{
			return NETWORK_ERR_SEND;
		}
		else
		{
			buf_size -= ret;
			off += ret;
		}
	}
	return NETWORK_OK;
}

static int send_udp(int sockfd, char *buf, int buf_size)
{
	return send_tcp(sockfd, buf, buf_size);
// 	if (!buf || !buf_size)
// 	{
// 		return NETWORK_ERR_INVALIDPARAM;
// 	}
// 	int off = 0;
// 	int ret = 0;
// 
// 	while(buf_size > 0)
// 	{
// 		ret = send(sockfd, buf + off, buf_size, 0);
// 		if (0 == ret || SOCKET_ERROR == ret)
// 		{
// 			return NETWORK_ERR_SEND;
// 		}
// 		else
// 		{
// 			buf_size -= ret;
// 			off += ret;
// 		}
// 	}
// 	return NETWORK_OK;
}

static int recv_broadcast()
{

	return NETWORK_OK;
}

static int recv_multicast()
{

	return NETWORK_OK;
}

int network_client_open(net_handle_t *handle, net_info_t net_info)
{
	if (!handle)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int ret = NETWORK_FALSE;
	int sock_id = 0;
	char dst_ip[DST_IP_MAX] = "";
	int sockfd = -1;

	if(parse_ip_from_str(net_info.m_dev_addr, dst_ip, sizeof(dst_ip)) < 0)
		return NETWORK_ERR_DEV_ADDR;

	ret = network_alloc(&s_network_ctx, &sock_id);
	if(NETWORK_OK != ret)
		return ret;

	pthread_mutex_init(&s_network_ctx.m_sock_param_arry_ptr[sock_id].m_mutex,NULL);

	if (NET_PROTOCOL_TYPE_TCP == net_info.m_pro_type)
	{
		if((sockfd = open_tcp(net_info)) < 0)
			return sockfd;
	}
	if (NET_PROTOCOL_TYPE_UDP == net_info.m_pro_type)
	{
		if((sockfd = open_udp(net_info)) < 0)
			return sockfd;
	}
	if (NET_PROTOCOL_TYPE_BROADCAST == net_info.m_pro_type)
	{
		if((sockfd = open_broadcast(net_info)) < 0)
			return sockfd;
	}
	if (NET_PROTOCOL_TYPE_MULTICAST == net_info.m_pro_type)
	{
		if((sockfd = open_multicast(net_info)) < 0)
			return sockfd;
	}
	s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock = sockfd;
	s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol = net_info.m_pro_type;
	*handle = (net_handle_t)(sock_id);

	return NETWORK_OK;
}

int network_client_close(net_handle_t handle)
{
	int sock_id = NET_HANDLE_2_SOCK_ID(handle);

	if (NET_PROTOCOL_TYPE_TCP == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		close_tcp(sock_id);
	}
	else if (NET_PROTOCOL_TYPE_UDP == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		close_udp(sock_id);
	}
	else if (NET_PROTOCOL_TYPE_BROADCAST == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		close_udp(sock_id);
	}
	else if (NET_PROTOCOL_TYPE_MULTICAST == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		close_udp(sock_id);
	}
	

	return NETWORK_OK;
}

int network_server_open(net_handle_t *handle, net_info_t net_info, server_callback callback)
{
	if (!handle)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int ret = NETWORK_FALSE;
	int sock_id = 0;
	char dst_ip[32] = "";
	int sockfd = -1;

	if(parse_ip_from_str(net_info.m_dev_addr, dst_ip, sizeof(dst_ip)) < 0)
		return NETWORK_ERR_DEV_ADDR;

	ret = network_alloc(&s_network_ctx, &sock_id);
	if(NETWORK_OK != ret)
		return ret;

	pthread_mutex_init(&s_network_ctx.m_sock_param_arry_ptr[sock_id].m_mutex,NULL);
	if (NET_PROTOCOL_TYPE_TCP == net_info.m_pro_type)
	{
		if((sockfd = open_tcp_server(net_info, callback)) < 0)
			return sockfd;
	}
	if (NET_PROTOCOL_TYPE_UDP == net_info.m_pro_type)
	{
		if((sockfd = open_udp_server(net_info, callback)) < 0)
			return sockfd;
	}
	if (NET_PROTOCOL_TYPE_BROADCAST == net_info.m_pro_type)
	{
		if((sockfd = open_broadcast_server(net_info, callback)) < 0)
			return sockfd;
	}
	if (NET_PROTOCOL_TYPE_MULTICAST == net_info.m_pro_type)
	{
		if((sockfd = open_multicast_server(net_info, callback)) < 0)
			return sockfd;
	}
	s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock = sockfd;
	s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol = net_info.m_pro_type;
	s_network_ctx.m_sock_param_arry_ptr[sock_id].m_callback = callback;
	*handle = (net_handle_t)(sock_id);

	return NETWORK_OK;
}

int network_server_close(net_handle_t handle)
{
	int sock_id = NET_HANDLE_2_SOCK_ID(handle);

	if (NET_PROTOCOL_TYPE_TCP == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		close_tcp(sock_id);
	}
	else if (NET_PROTOCOL_TYPE_UDP == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		close_udp(sock_id);
	}
	else if (NET_PROTOCOL_TYPE_BROADCAST == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		close_udp(sock_id);
	}
	else if (NET_PROTOCOL_TYPE_MULTICAST == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		close_udp(sock_id);
	}
	s_network_ctx.m_sock_param_arry_ptr[sock_id].m_callback = NULL;

	return NETWORK_OK;
}

int network_server_recv(int sockfd, char *recv_buf, int buf_size)
{	
	if (!recv_buf || !buf_size)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	return	recv_tcp(sockfd, recv_buf, buf_size);
}

int network_server_send(int sockfd, char *send_buf, int buf_size)
{
	if (!send_buf || !buf_size)
	{
		return NETWORK_ERR_INVALIDPARAM;
	}
	int ret = -1;

	return send_tcp(sockfd, send_buf, buf_size);
}

int network_server_check_timeout(int sockfd, int millisecond)
{
	fd_set read_set;
	struct timeval tvs;
	int ret = -1;

	FD_ZERO(&read_set);
	FD_SET(sockfd, &read_set);
	tvs.tv_sec = millisecond / 1000;
	tvs.tv_usec = (millisecond % 1000) * 1000;

	ret = select(sockfd + 1, &read_set, NULL, NULL, &tvs);
	if (ret < 0)
	{
		return NETWORK_ERR_SOCK_ABNORMAL;
	}
	if (0 == ret)
	{
		return NETWORK_ERR_TIMEOUT;
	}
	return NETWORK_OK;
}

int network_client_recv(net_handle_t handle, char *recv_buf, int buf_size)
{
	int sock_id = NET_HANDLE_2_SOCK_ID(handle);
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock;

	if (NET_PROTOCOL_TYPE_TCP == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		return recv_tcp(sockfd, recv_buf, buf_size);
	}
	else if (NET_PROTOCOL_TYPE_UDP == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		return recv_udp(sockfd, recv_buf, buf_size);
	}
	else if (NET_PROTOCOL_TYPE_BROADCAST == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		return recv_udp(sockfd, recv_buf, buf_size);
	}
	else if (NET_PROTOCOL_TYPE_MULTICAST == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		return recv_udp(sockfd, recv_buf, buf_size);
	}
}

int network_client_send(net_handle_t handle, char *send_buf, int buf_size)
{
	int sock_id = NET_HANDLE_2_SOCK_ID(handle);
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock;
	int ret = -1;

	pthread_mutex_lock(&s_network_ctx.m_sock_param_arry_ptr[sock_id].m_mutex);
	if (NET_PROTOCOL_TYPE_TCP == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		ret = send_tcp(sockfd, send_buf, buf_size);
	}
	else if (NET_PROTOCOL_TYPE_UDP == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		ret = send_udp(sockfd, send_buf, buf_size);
	}
	else if (NET_PROTOCOL_TYPE_BROADCAST == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		ret = send_udp(sockfd, send_buf, buf_size);
	}
	else if (NET_PROTOCOL_TYPE_MULTICAST == s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock_protocol)
	{
		ret = send_udp(sockfd, send_buf, buf_size);
	}

client_send:
	pthread_mutex_unlock(&s_network_ctx.m_sock_param_arry_ptr[sock_id].m_mutex);
	return ret;
}

int network_client_check_timeout(net_handle_t handle, int millisecond)
{
	int sock_id = NET_HANDLE_2_SOCK_ID(handle);
	int sockfd = s_network_ctx.m_sock_param_arry_ptr[sock_id].m_sock;
	fd_set read_set;
	struct timeval tvs;
	int ret = -1;
	FD_ZERO(&read_set);
	FD_SET(sockfd, &read_set);
	tvs.tv_sec = millisecond / 1000;
	tvs.tv_usec = (millisecond % 1000) * 1000;
	ret = select(sockfd + 1, &read_set, NULL, NULL, &tvs);
	if (ret < 0)
	{
		return NETWORK_ERR_SOCK_ABNORMAL;
	}
	if (0 == ret)
	{
		return NETWORK_ERR_TIMEOUT;
	}
	return NETWORK_OK;
}
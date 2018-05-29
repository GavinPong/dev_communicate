#ifndef __NETWORK_H__
#define __NETWORK_H__

#define ADDR_MAX 64

//return value
#define NETWORK_OK					1			//成功
#define NETWORK_FALSE				0			//失败
#define NETWORK_ERR_INVALIDPARAM	-1			//非法参数
#define NETWORK_ERR_SOCKED			-2			//创建套接字失败
#define NETWORK_ERR_CONNECT			-3			//连接失败
#define NETWORK_ERR_BIND			-4			//连接失败
#define NETWORK_ERR_SEND			-5			//发送失败
#define NETWORK_ERR_RECV			-6			//接收失败
#define NETWORK_ERR_INVALSOCKET		-7			//非法套接字
#define NETWORK_ERR_NOMEMORY		-8			//内存不够
#define NETWORK_ERR_PROTOCOL		-9			//非法协议
#define NETWORK_ERR_TIMEOUT			-10			//超时
#define NETWORK_ERR_DEV_ADDR		-11			//设备地址异常
#define NETWORK_ERR_THREAD			-12			//创建线程资源失败
#define NETWORK_ERR_SOCK_ABNORMAL	-13			//套接字异常

typedef enum _net_addr_family_e
{
	NET_AF_UNSPEC   =    0,               /* unspecified */
	NET_AF_UNIX     =    1,               /* local to host (pipes, portals) */
	NET_AF_INET     =    2,               /* internetwork: UDP, TCP, etc. */
	NET_AF_IMPLINK  =    3,               /* arpanet imp addresses */
	NET_AF_PUP      =    4,               /* pup protocols: e.g. BSP */
	NET_AF_CHAOS    =    5,               /* mit CHAOS protocols */
	NET_AF_IPX      =    6,               /* IPX and SPX */
	NET_AF_NS       =    6,               /* XEROX NS protocols */
	NET_AF_ISO      =    7,               /* ISO protocols */
	NET_AF_OSI      =    NET_AF_ISO,      /* OSI is ISO */
	NET_AF_ECMA     =    8,               /* european computer manufacturers */
	NET_AF_DATAKIT  =    9,               /* datakit protocols */
	NET_AF_CCITT    =    10,              /* CCITT protocols, X.25 etc */
	NET_AF_SNA      =    11,              /* IBM SNA */
	NET_AF_DECnet   =    12,              /* DECnet */
	NET_AF_DLI      =    13,              /* Direct data link interface */
	NET_AF_LAT      =    14,              /* LAT */
	NET_AF_HYLINK   =    15,              /* NSC Hyperchannel */
	NET_AF_APPLETALK =   16,              /* AppleTalk */
	NET_AF_NETBIOS   =   17,              /* NetBios-style addresses */
	NET_AF_VOICEVIEW =   18,              /* VoiceView */
	NET_AF_FIREFOX   =   19,              /* FireFox */
	NET_AF_UNKNOWN1  =   20,              /* Somebody is using this! */
	NET_AF_BAN       =   21,              /* Banyan */

	NET_AF_MAX       =   22,
}net_addr_family_e;

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum _net_protocol_type_e
{
	NET_PROTOCOL_TYPE_NULL,
	NET_PROTOCOL_TYPE_TCP,				//tcp
	NET_PROTOCOL_TYPE_UDP,				//udp
	NET_PROTOCOL_TYPE_BROADCAST,		//广播
	NET_PROTOCOL_TYPE_MULTICAST,		//组播
}net_protocol_type_e;

typedef struct _net_info_s
{
	char m_dev_addr[ADDR_MAX];			//设备地址
	unsigned short m_dev_port;			//设备端口
	unsigned short m_family;			//协议族
	net_protocol_type_e m_pro_type;		//网络协议类型
	unsigned short m_timeout;			//超时时间,单位:s
	unsigned short m_sendbuf_size;		//发送缓存区大小，为0的话，为系统默认值，建议使用0
	unsigned short m_recvbuf_size;		//接收缓存区大小，为0的话，为系统默认值，建议使用0
}net_info_t, *pnet_info_t;

typedef struct _addr_info_s
{
	int m_sock;							//通讯套接字
	int m_data_len;						//data数据长度
	char data[];						//存放的struct sockaddr_in数据;					
}addr_info_t, *paddr_info_t;

typedef void* net_handle_t;
typedef int (*server_callback)(paddr_info_t paddr_info);

//open 和 close必须成对使用，否则会造成资源泄漏
int network_client_open(net_handle_t *handle, net_info_t net_info);
int network_client_close(net_handle_t handle);
int network_client_recv(net_handle_t handle, char *recv_buf, int buf_size);
int network_client_send(net_handle_t handle, char *send_buf, int buf_size);
int network_client_check_timeout(net_handle_t handle, int millisecond);

int network_server_open(net_handle_t *handle, net_info_t net_info, server_callback callback);
int network_server_close(net_handle_t handle);
int network_server_recv(int sockfd, char *recv_buf, int buf_size);
int network_server_send(int sockfd, char *send_buf, int buf_size);
int network_server_check_timeout(int sockfd, int millisecond);

#ifdef __cplusplus
}
#endif

#endif
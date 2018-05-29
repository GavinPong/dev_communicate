#ifndef __NETWORK_H__
#define __NETWORK_H__

#define ADDR_MAX 64

//return value
#define NETWORK_OK					1			//�ɹ�
#define NETWORK_FALSE				0			//ʧ��
#define NETWORK_ERR_INVALIDPARAM	-1			//�Ƿ�����
#define NETWORK_ERR_SOCKED			-2			//�����׽���ʧ��
#define NETWORK_ERR_CONNECT			-3			//����ʧ��
#define NETWORK_ERR_BIND			-4			//����ʧ��
#define NETWORK_ERR_SEND			-5			//����ʧ��
#define NETWORK_ERR_RECV			-6			//����ʧ��
#define NETWORK_ERR_INVALSOCKET		-7			//�Ƿ��׽���
#define NETWORK_ERR_NOMEMORY		-8			//�ڴ治��
#define NETWORK_ERR_PROTOCOL		-9			//�Ƿ�Э��
#define NETWORK_ERR_TIMEOUT			-10			//��ʱ
#define NETWORK_ERR_DEV_ADDR		-11			//�豸��ַ�쳣
#define NETWORK_ERR_THREAD			-12			//�����߳���Դʧ��
#define NETWORK_ERR_SOCK_ABNORMAL	-13			//�׽����쳣

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
	NET_PROTOCOL_TYPE_BROADCAST,		//�㲥
	NET_PROTOCOL_TYPE_MULTICAST,		//�鲥
}net_protocol_type_e;

typedef struct _net_info_s
{
	char m_dev_addr[ADDR_MAX];			//�豸��ַ
	unsigned short m_dev_port;			//�豸�˿�
	unsigned short m_family;			//Э����
	net_protocol_type_e m_pro_type;		//����Э������
	unsigned short m_timeout;			//��ʱʱ��,��λ:s
	unsigned short m_sendbuf_size;		//���ͻ�������С��Ϊ0�Ļ���ΪϵͳĬ��ֵ������ʹ��0
	unsigned short m_recvbuf_size;		//���ջ�������С��Ϊ0�Ļ���ΪϵͳĬ��ֵ������ʹ��0
}net_info_t, *pnet_info_t;

typedef struct _addr_info_s
{
	int m_sock;							//ͨѶ�׽���
	int m_data_len;						//data���ݳ���
	char data[];						//��ŵ�struct sockaddr_in����;					
}addr_info_t, *paddr_info_t;

typedef void* net_handle_t;
typedef int (*server_callback)(paddr_info_t paddr_info);

//open �� close����ɶ�ʹ�ã�����������Դй©
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
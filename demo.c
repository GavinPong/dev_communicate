#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/resource.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <sys/time.h>
#include "comport.h"
#include "network_tools.h"
#include "network.h"
#include "base64.h"

#ifndef D_PRT
#define D_PRT  printf
#endif
#define NET_INT_MODULE  52

#define INTERFACE_NAME "eth0"
#define DHCPC_FILE "/data/oem/TouchRB/dhcp_stat.json"

static const char POST_AUTH_HEADER[] =
	"POST %s HTTP/1.1\r\n"
	"HOST:%s\r\n"
	"Referer:http://%s/\r\n"
	"Cache-Control: no-cache\r\n"
	"Authorization:Basic %s\r\n"
	"Content-Type:application/json\r\n"
	"Connection:Keep-Alive\r\n"
	"Content-Length:%d\r\n\r\n"
	"%s\r\n";

static const char POST_BODY[] =
	"%s\r\n";


typedef enum _app_stat_e{
	APP_STAT_RUN,
	APP_STAT_EXIT,
}app_stat_e;

typedef struct _global_ctx_s{
	app_stat_e m_app_quit;
	comport_handle_t m_comport_handle;
	net_handle_t m_net_handle;
	net_info_t m_net_info;
	pthread_mutex_t m_mutex;
	network_param_t m_network_param;
	char m_is_get_ip;		
}global_ctx_t;

global_ctx_t g_global_ctx;

void print_buf(unsigned char *buf, int print_size){
	int i = 0;
	for (i = 0; i < print_size; i++)
	{
		D_PRT(":0x%02x", buf[i]);
	}
	D_PRT("\n");
}

void signal_handler(int signal){
	D_PRT("application recv signal %d from system\n", signal);
	g_global_ctx.m_app_quit = APP_STAT_EXIT;
}

void register_all_signal_handler(){
	int i = 0;
	for (;i < 32; i++)
	{
		signal(i, signal_handler);
	}
}

int set_or_get_dhcp_stat(int *dhcp_stat, int set_or_get_opt){
	if (!dhcp_stat)
	{
		return -1;
	}
	FILE *fd = NULL;
	if (1 == set_or_get_opt)
	{
		fd = fopen(DHCPC_FILE, "w+");
	}
	else
	{
		fd = fopen(DHCPC_FILE, "r");
	}
	if (!fd)
	{
		return -2;
	}
	if (1 == set_or_get_opt)
	{
		fwrite(dhcp_stat, 1, 1, fd);
	}
	else
	{
		fread(dhcp_stat, 1, 1, fd);
	}
	fclose(fd);
	return 0;
}

int vc_authorization_base64_encode(char *out_base64, char *username, char *password)
{
	if (!out_base64 || (!username && !password))
	{
		return -1;
	}
	unsigned char authorrization[1024] = "";

	if (username || password)
	{
		if (username && password)
		{
			snprintf((char *)authorrization, sizeof(authorrization), "%s:%s", username, password);
		}
		else if (username)
		{
			snprintf((char *)authorrization, sizeof(authorrization), "%s", username);
		}
		else
		{
			snprintf((char *)authorrization, sizeof(authorrization), ":%s", password);
		}
		base64_encode(authorrization, out_base64, strlen((char *)authorrization));
	}

	return 0;
}


//====================Network=========================
int set_Interaction_netparam_by_net(net_info_t net_info, network_param_t network_para, int re_cnt){
	char base64[1024] = "";
	char body[1024] = "";
	char http_header[20148] = "";
	int body_size = 0;
	int ret = -1;
	char ip_str[16] = "";
	char mask[16] = "";
	char gatway[16] = "";
	char dns[16] = "";

	ip_to_str(network_para.m_ip, ip_str);
	ip_to_str(network_para.m_mask, mask);
	ip_to_str(network_para.m_gateway, gatway);
	ip_to_str(network_para.m_dns, dns);
redo:
	if (re_cnt-- <= 0)
	{
		return 0;
	}
	if (NETWORK_OK != network_client_open(&g_global_ctx.m_net_handle, net_info))
	{
		usleep(300 * 1000);
		goto redo;
	}
#if 0

	struct  timeval tv_start, tv_stop; 
	char tmp_buf[1024] = "";
	gettimeofday(&tv_start, NULL);
	if(network_client_recv(g_global_ctx.m_net_handle, tmp_buf, sizeof(tmp_buf)) <= 0)
	{
		gettimeofday(&tv_stop, NULL);
		D_PRT("%s->%d:used time:%ld\n", __FILE__, __LINE__,tv_stop.tv_sec - tv_start.tv_sec);
		network_client_close(g_global_ctx.m_net_handle);
		g_global_ctx.m_net_handle = 0;
		return -3;
	}
#endif
	if (network_para.m_en_dhcp)
	{
		snprintf(body, sizeof(body), "{\"dhcp\":true,\"ip\":\"%s\",\"mask\":\"%s\",\"gw\":\"%s\",\"dns\":\"%s\"}", ip_str, mask, gatway, dns);
	}
	else
	{
		snprintf(body, sizeof(body), "{\"dhcp\":false,\"ip\":\"%s\",\"mask\":\"%s\",\"gw\":\"%s\",\"dns\":\"%s\"}", ip_str, mask, gatway, dns);
	}
	body_size = strlen(body);
	
	vc_authorization_base64_encode(base64, "admin", "admin");
	snprintf(http_header, sizeof(http_header), POST_AUTH_HEADER, "/api/v1/network/config-set/", net_info.m_dev_addr, net_info.m_dev_addr,base64, body_size, body);
	D_PRT("%s->%d:%s\n", __FILE__, __LINE__, http_header);
	if(NETWORK_OK != network_client_send(g_global_ctx.m_net_handle, http_header, strlen(http_header)))
	{
		network_client_close(g_global_ctx.m_net_handle);
		g_global_ctx.m_net_handle = 0;
		usleep(300 * 1000);
		goto redo;
	}
	network_client_close(g_global_ctx.m_net_handle);
	g_global_ctx.m_net_handle = 0;
	return 1;
}
//====================================================


int find_mcu_network_code(unsigned char *code,int len,int *pos,int* plen){
	if (len < 23)
	{
		return 0;
	}

	int i = 0;
	int datelen = 0;
	for (i=0;i<len;i++)
	{
		if (len - i < 6)
		{
			return 0;
		}

		if (code[i] == 0xee && code[i+1] == 0x60)
		{
			datelen = code[i+3];
			if (datelen <= len - i)
			{
				if (code[i+datelen-1] == 0xfc && code[i + datelen - 2] == 0xfb)
				{
					*pos = i;
					*plen = datelen;
					return 1;
				}
			}
		}
	}

	return 0;
}

int send_meet_network_to_serail(network_param_t meet_netwrok, char set_or_get){
#if 1
	char ip_str[16] = "";
	char mask[16] = "";
	char gatway[16] = "";
	char dns[16] = "";

	ip_to_str(meet_netwrok.m_ip, ip_str);
	ip_to_str(meet_netwrok.m_mask, mask);
	ip_to_str(meet_netwrok.m_gateway, gatway);
	ip_to_str(meet_netwrok.m_dns, dns);
	printf("%s->%d:dhcp:%d	ip:%s	mask:%s		gatway:%s	dns:%s\n",__FILE__, __LINE__,meet_netwrok.m_en_dhcp, ip_str, mask, gatway, dns);
#endif
	char ip_buf[64] = "";
	unsigned int *ip = NULL;
	char *end_fame = NULL;

	ip_buf[0] = 0xee;
	ip_buf[1] = 0x60;
	if (1 == set_or_get)
	{
		ip_buf[2] = 0x01;	//设置请求
	}
	else
	{
		ip_buf[2] = 0x00;	//获取请求
	}
	ip_buf[3] = 0x17;
	ip_buf[4] = meet_netwrok.m_en_dhcp;
	ip = (unsigned int *)&ip_buf[5];
	*ip = meet_netwrok.m_ip;
	*(ip + 1) = meet_netwrok.m_mask;
	*(ip + 2) = meet_netwrok.m_gateway;
	*(ip + 3) = meet_netwrok.m_dns;
	end_fame = (char *)(ip + 4);
	end_fame[0] = 0xfb;
	end_fame[1] = 0xfc;

	comport_transfer_send(g_global_ctx.m_comport_handle, ip_buf, ip_buf[3]);
	return 0;
}

void init_global_ctx(){
	g_global_ctx.m_app_quit = APP_STAT_RUN;
	g_global_ctx.m_comport_handle = 0;
	g_global_ctx.m_net_info.m_dev_port = 80;
	strncpy(g_global_ctx.m_net_info.m_dev_addr, "127.0.0.1", sizeof(g_global_ctx.m_net_info.m_dev_addr));
	g_global_ctx.m_net_info.m_family = NET_AF_INET;
	g_global_ctx.m_net_info.m_pro_type = NET_PROTOCOL_TYPE_TCP;
	g_global_ctx.m_net_info.m_timeout = 2;
	pthread_mutex_init(&g_global_ctx.m_mutex, NULL);
	get_network(&g_global_ctx.m_network_param, INTERFACE_NAME);

}

void uninit_global_ctx(){
	pthread_mutex_destroy(&g_global_ctx.m_mutex);

}

int sync_cur_network_param_to_dev(network_param_t network_param){
	int dhcp_stat = 0;

	g_global_ctx.m_network_param = network_param;
	set_or_get_dhcp_stat(&dhcp_stat, 0);
	network_param.m_en_dhcp = dhcp_stat;
	pthread_mutex_lock(&g_global_ctx.m_mutex);
	send_meet_network_to_serail(network_param, 0);
	pthread_mutex_unlock(&g_global_ctx.m_mutex);
}

void *ip_change_process(void *param){
	network_param_t network_param;
	int dhcp_stat = 0;

	while(APP_STAT_RUN == g_global_ctx.m_app_quit){
		get_network(&network_param, INTERFACE_NAME);
		if (network_param.m_ip != g_global_ctx.m_network_param.m_ip ||	\
			network_param.m_mask != g_global_ctx.m_network_param.m_mask ||	\
			network_param.m_gateway != g_global_ctx.m_network_param.m_gateway ||	\
			network_param.m_dns != g_global_ctx.m_network_param.m_dns || 0 == g_global_ctx.m_is_get_ip)
		{
			sync_cur_network_param_to_dev(network_param);
		}
		usleep(1000 * 1000);
	}

	return NULL;
}

int main(){
	int ret = 0;
	int i = 0;
	int rec_len = 1;
	int data_len = 0x17;
	int offset = 0;
	int find_offset = 0;
	int pos = 0;
	int cmd_len = 0;
	unsigned char buf[256] = "";
	network_param_t network_param;
	network_param_t *p_c9_network = &network_param;
	char ip_str[16] = "";
	char mask[16] = "";
	char gatway[16] = "";
	char dns[16] = "";
	char net_info[256] = "";
	int dhcp_stat = 0;
	pthread_t thread;
	comport_cfg_t comport_cfg;

	init_global_ctx();
#if 0
	ret = get_network(&network_param, INTERFACE_NAME);
	ip_to_str(network_param.m_ip, ip_str);
	ip_to_str(network_param.m_mask, mask);
	ip_to_str(network_param.m_gateway, gatway);
	ip_to_str(network_param.m_dns, dns);
	D_PRT("%s->%d:%d-%s-%s-%s-%s\n", __FILE__, __LINE__, network_param.m_en_dhcp, ip_str, mask, gatway, dns);
	network_param_tmp = network_param;
	network_param_tmp.m_gateway = network_param.m_gateway + 0x01000000;
	ret = set_network(network_param_tmp, "eth0");
	ret = get_network(&network_param_tmp, INTERFACE_NAME);
	ip_to_str(network_param_tmp.m_ip, ip_str);
	ip_to_str(network_param_tmp.m_mask, mask);
	ip_to_str(network_param_tmp.m_gateway, gatway);
	ip_to_str(network_param_tmp.m_dns, dns);
	D_PRT("%s->%d:%d-%s-%s-%s-%s\n", __FILE__, __LINE__, network_param.m_en_dhcp, ip_str, mask, gatway, dns);
#endif

	register_all_signal_handler();

	comport_module_init();

	if(COMPORT_OK != comport_open(&g_global_ctx.m_comport_handle, "/dev/ttyO4"))
		//if(COMPORT_OK != comport_open(&comport_handle, "/dev/ttyAMA2"))
	{
		D_PRT("%s->%d:comport_open failed\n", __FILE__, __LINE__);
		return COMPORT_ERR_OPEN;
	}
#if 1
	comport_cfg.m_baud_rate = BAUD_RATE_115200;
	comport_cfg.m_data_bit = DATA_BIT_8;
	comport_cfg.m_flow_control = FLOW_CONTROL_NONE;
	comport_cfg.m_parity = PARITY_NONE;
	comport_cfg.m_stop_bit = STOPBITS_1;
	comport_set_cfg(g_global_ctx.m_comport_handle, comport_cfg);
#endif
	D_PRT("%s->%d:open /dev/ttyO4 success\n", __FILE__, __LINE__);
	if (pthread_create(&thread, NULL, ip_change_process, NULL))
	{
		comport_close(g_global_ctx.m_comport_handle);
		comport_module_uninit();
		D_PRT("create ip_change_process thread is failed\n");
		return -2;
	}
	pthread_detach(thread);
#if 0
	if(set_Interaction_netparam_by_net(g_global_ctx.m_net_info, network_param, 3) > 0){
		dhcp_stat = network_param.m_en_dhcp;
		set_or_get_dhcp_stat(&dhcp_stat, 1);
	}
#endif
	while(APP_STAT_RUN == g_global_ctx.m_app_quit){
#if 0
	char buf_tmp[] = {1,2,3,4,5,6,7,8,11,12,13,14,15,16,17,18,21,22,23,24,25,66,77,88};
	D_PRT("%s->%d:send size:%d\n", __FILE__, __LINE__, comport_transfer_send(g_global_ctx.m_comport_handle, (char *)(buf_tmp), sizeof(buf_tmp)));
	usleep(500 * 1000);
	continue;
#endif
#if 0
		ret = comport_transfer_recv(g_global_ctx.m_comport_handle, (char *)(buf+offset), sizeof(buf), 50);
		if(ret >= 0)
		{
			D_PRT("%s-%d:ret:%d\n", __FILE__, __LINE__, ret);
			D_PRT("%s->%d", __FILE__, __LINE__);
			print_buf(buf, offset + ret);
		}
		continue;
#endif
		ret = comport_transfer_recv(g_global_ctx.m_comport_handle, (char *)(buf+offset), rec_len, 50);
		if (COMPORT_ERR_TIMEOUT == ret || 0 == ret)
		{
			//D_PRT("%s->%d:exit app %d\n", __FILE__, __LINE__, app_quit);
			continue;
		}
		if (ret < 0)
		{
			D_PRT("%s->%d:exit app %d\n", __FILE__, __LINE__, g_global_ctx.m_app_quit);
			break;
		}
		D_PRT("%s->%d", __FILE__, __LINE__);
		print_buf(buf, offset + ret);
		if (1 == rec_len && 0 == offset && 0xee != buf[0])
		{
			rec_len = 1;
			offset = 0;
			continue;
		}
		if (1 == rec_len && 1 == offset && 0x60 != buf[1])
		{
			rec_len = 1;
			offset = 0;
			continue;
		}
		if (1 == rec_len && 3 == offset && 0x17 != buf[3])	//只验收网络码
		{
			rec_len = 1;
			offset = 0;
			continue;
		}
		if (3 == offset)
		{
			rec_len = 0x17 - 4;
			offset = 4;
			D_PRT("%s->%d:ret:%d data_len:%d offset:%d rec_len:%d\n", __FILE__, __LINE__, ret, data_len, offset, rec_len);
			continue;
		}
		offset += ret;
		D_PRT("%s->%d:ret:%d data_len:%d offset:%d rec_len:%d\n", __FILE__, __LINE__, ret, data_len, offset, rec_len);
		if (offset <= 3)
		{
			continue;
		}
		if (data_len != offset)
		{
			D_PRT("%s->%d", __FILE__, __LINE__);
			print_buf(buf, offset);
			rec_len -= ret;
			D_PRT("%s->%d:ret:%d data_len:%d offset:%d rec_len:%d\n", __FILE__, __LINE__, ret, data_len, offset, rec_len);
			continue;
		}
		D_PRT("%s->%d", __FILE__, __LINE__);
		print_buf(buf, offset);
		find_offset = 0;
		while(APP_STAT_RUN == g_global_ctx.m_app_quit && find_mcu_network_code(buf + find_offset, data_len - find_offset, &pos, &cmd_len))
		{
			if (cmd_len < 17)
			{
				continue;
			}
			D_PRT("111\n");
			if (0x01 == *(buf+find_offset+pos+2))
			{
				p_c9_network = &network_param;
				p_c9_network->m_en_dhcp = *(buf+find_offset+pos+4);
				p_c9_network->m_ip = *((unsigned long *)(buf+find_offset+pos+4 + 1));
				p_c9_network->m_mask = *((unsigned long *)(buf+find_offset+pos+4 + 1 + 1 * sizeof(unsigned long)));
				p_c9_network->m_gateway = *((unsigned long *)(buf+find_offset+pos+4 + 1 + 2 * sizeof(unsigned long)));
				p_c9_network->m_dns = *((unsigned long *)(buf+find_offset+pos+4 + 1 + 3 * sizeof(unsigned long)));

				ip_to_str(p_c9_network->m_ip, ip_str);
				ip_to_str(p_c9_network->m_mask, mask);
				ip_to_str(p_c9_network->m_gateway, gatway);
				ip_to_str(p_c9_network->m_dns, dns);
				D_PRT("%s->%d:%d-%s-%s-%s-%s\n", __FILE__, __LINE__, p_c9_network->m_en_dhcp, ip_str, mask, gatway, dns);
				//set_network(*p_c9_network, INTERFACE_NAME);
				set_or_get_dhcp_stat(&dhcp_stat, 0);
				if(set_Interaction_netparam_by_net(g_global_ctx.m_net_info, *p_c9_network, 3) > 0){
					dhcp_stat = p_c9_network->m_en_dhcp;
					set_or_get_dhcp_stat(&dhcp_stat, 1);
				}
				usleep(1000 * 1000);
				get_network(p_c9_network, INTERFACE_NAME);
				p_c9_network->m_en_dhcp = dhcp_stat;
				pthread_mutex_lock(&g_global_ctx.m_mutex);
				send_meet_network_to_serail(network_param, 0);
				pthread_mutex_unlock(&g_global_ctx.m_mutex);
				//主动刷新ip
			}
			if (0x00 == *(buf+find_offset+pos+2))
			{
				g_global_ctx.m_is_get_ip = 1;
				get_network(p_c9_network, INTERFACE_NAME);
				set_or_get_dhcp_stat(&dhcp_stat, 0);
				p_c9_network->m_en_dhcp = dhcp_stat;
				pthread_mutex_lock(&g_global_ctx.m_mutex);
				send_meet_network_to_serail(network_param, 0);
				pthread_mutex_unlock(&g_global_ctx.m_mutex);

			}
			find_offset += pos + cmd_len;
		}
		//重新开始接受
		rec_len = 1;
		offset = 0;
		memset(buf, 0, ret);
	}
	D_PRT("%s->%d:exit app %d\n", __FILE__, __LINE__, g_global_ctx.m_app_quit);
	comport_close(g_global_ctx.m_comport_handle);
	comport_module_uninit();


	return 0;
}
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
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
#include <linux/route.h>
#include "network_tools.h"

#ifndef D_PRT
#define D_PRT  printf
#endif

int str_to_ip(char *pIpaddr){
	int i = 0;
	int dwIp = 0;
	char *pStr;
	char *pChar;

	union s_endian {char c1; int i;}s_e = {1};
	int bLittle = s_e.c1 == 1;

	if (bLittle) {
		for (pStr = pIpaddr, i = 0; i < 4; ++i) {
			dwIp += strtol(pStr, &pChar, 10) << i*8;
			pStr = pChar + 1;
		}
	} else {
		for (pStr = pIpaddr, i = 0; i < 4; ++i) {
			dwIp += strtol(pStr, &pChar, 10) << (3-i)*8;
			pStr = pChar + 1;
		}
	}

	return dwIp;
}

void ip_to_str(unsigned long dwIp, char *pIpaddr){
	int i = 0;
	unsigned char pIp[4];
	for ( i =0; i < 4; i ++)
	{
		pIp[i] = ( dwIp >> i * 8 ) & 0xFF;
	}
	sprintf(pIpaddr, "%d.%d.%d.%d", pIp[0], pIp[1], pIp[2], pIp[3]);
}

int get_dsthost_gateway(unsigned long *gateway, unsigned int dest_host, char *interface_name){
	FILE *fp = NULL;
	char buf[256] = "";
	char Iface[16] = "";	
	unsigned int Destination = 0, Gateway = 0, Flags = 0, RefCnt = 0,	Use = 0, Metric = 0, Mask = 0,	MTU = 0, Window = 0, IRTT = 0;
	char first_flag = 0;

	if (!interface_name)
	{
		return -1;
	}
	fp =fopen("/proc/net/route", "r+");
	if (!fp)
	{
		return -1;
	}
	while(fgets(buf, sizeof(buf), fp))
	{
		if (strlen(buf))
		{
			sscanf(buf, "%s  %x  %x  %x  %x  %x  %x  %x  %x  %x  %x", Iface, &Destination, &Gateway, &Flags, &RefCnt,	&Use, &Metric, &Mask, &MTU, &Window, &IRTT);
			if(!strcmp(buf, interface_name))
			{
				memset(buf, 0, strlen(buf));
				continue;
			}
			if (0 == first_flag)
			{
				first_flag = 1;
				*gateway = Gateway;
			}
			if (dest_host == Destination)
			{
				*gateway = Gateway;
				break;
			}
		}
		memset(buf, 0, strlen(buf));
	}
	fclose(fp);
	if (1 == first_flag)
		return 1;
	return 0;
}

int set_dsthost_gateway(unsigned long gateway, unsigned int dest_host, char *interface_name){
	int sockfd;  
	struct rtentry  rt;
	struct sockaddr_in *p_sock_addr = NULL;   
	unsigned long old_gateway;
	if (!interface_name)  
	{  
		return -1;  
	}  
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)  
	{  
		return -2;  
	}  

	memset(&rt, 0, sizeof(struct rtentry));
	p_sock_addr = (struct sockaddr_in *)&rt.rt_dst;
	p_sock_addr->sin_family = AF_INET;
	p_sock_addr = (struct sockaddr_in *)&rt.rt_genmask;
	p_sock_addr->sin_family = AF_INET;
	p_sock_addr = (struct sockaddr_in *)&rt.rt_gateway;
	p_sock_addr->sin_family = AF_INET;

	//delete the old route
	get_dsthost_gateway(&old_gateway, dest_host, interface_name);
	p_sock_addr->sin_addr.s_addr = old_gateway;
	if(0 != ioctl(sockfd, SIOCDELRT, &rt))
	{
		if(ESRCH != errno)
		{   
			perror("\n");
			close(sockfd); 
			return -3;     
		}
	}
	//add a new route
	rt.rt_flags = RTF_GATEWAY;
	p_sock_addr->sin_addr.s_addr = gateway;
	if (ioctl(sockfd, SIOCADDRT, &rt)<0)
	{
		close(sockfd);
		return -4;
	}
	close(sockfd);
	return 1;
}

int get_dns(unsigned long *dns1, unsigned long *dns2){
	if (!dns1)
	{
		return -1;
	}
	*dns1 = 0;
	if (dns2)
	{
		*dns2 = 0;
	}
	FILE *fp = NULL;
	char buf[256] = "";
	char Iface[16] = "";	
	char first_flag = 0;
	fp =fopen("/etc/resolv.conf", "r+");
	if (!fp)
	{
		return -1;
	}
	while(fgets(buf, sizeof(buf), fp))
	{
		if (strlen(buf) && !strncmp(buf, "nameserver ", strlen("nameserver ") - 1))
		{
			if (dns1 && !(*dns1))
			{
				*dns1 = str_to_ip(buf + strlen("nameserver "));
				continue;
			}
			if (dns2)
			{
				*dns2 = str_to_ip(buf + strlen("nameserver "));
				break;
			}
		}
		memset(buf, 0, strlen(buf));
	}
	fclose(fp);
	if (*dns1)
		return 1;
	return 0;
}

//-1:param is error -2:socket create failed -3:get ip failed -4:get mask failed -5:get gateway failed -6:get dns failed
int get_network(network_param_t *pnetwork_param, char *interface_name){
	if (!pnetwork_param || !interface_name)
	{
		return -1;
	}
	int sockfd;  
	char ipaddr[16];  
	struct   sockaddr_in *sin = NULL;  
	struct   ifreq ifr_ip;   
	unsigned int dest_host = 0;//0:default host

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)  
	{  
		return -2;  
	}  

	memset(&ifr_ip, 0, sizeof(ifr_ip));     
	strncpy(ifr_ip.ifr_name, interface_name, sizeof(ifr_ip.ifr_name) - 1);     
	//ip
	if( ioctl( sockfd, SIOCGIFADDR, &ifr_ip) < 0 )     
	{   
		close(sockfd); 
		return -3;     
	}       
	sin = (struct sockaddr_in *)&ifr_ip.ifr_addr;     
	pnetwork_param->m_ip = sin->sin_addr.s_addr;
	//mask
	memset(&ifr_ip.ifr_addr, 0, sizeof(ifr_ip.ifr_addr));     
	if( (ioctl( sockfd, SIOCGIFNETMASK, &ifr_ip ) ) < 0 )   
	{   
		close(sockfd); 
		return -4;  
	}    
	sin = (struct sockaddr_in *)&ifr_ip.ifr_addr;     
	pnetwork_param->m_mask = sin->sin_addr.s_addr; 
	//gateway
	if(get_dsthost_gateway(&pnetwork_param->m_gateway, dest_host, interface_name) <= 0)
	{   
		close(sockfd); 
		return -5;  
	}    
	//get dns
	if(get_dns(&pnetwork_param->m_dns, NULL) <= 0)
	{   
		close(sockfd); 
		return -6;  
	}    
	close(sockfd);  
	return 0;
}

int set_network(network_param_t network_param, char *interface_name){
	int sockfd;  
	char ipaddr[16];  
	struct   sockaddr_in *sin = NULL;  
	struct   ifreq ifr_ip;  
	struct rtentry  rt;
	struct sockaddr_in *p_sock_addr = NULL;
	network_param_t tmp_network_param;
	unsigned int dest_host = 0;//0:default host
	int ret = -1;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)  
	{  
		return -2;  
	}  
	sin = (struct sockaddr_in *)&ifr_ip.ifr_addr; 
	memset(&ifr_ip, 0, sizeof(ifr_ip));     
	strncpy(ifr_ip.ifr_name, interface_name, sizeof(ifr_ip.ifr_name) - 1);  
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = network_param.m_ip;
	//ip
	if( ioctl( sockfd, SIOCSIFADDR, &ifr_ip) < 0 )     
	{   
		close(sockfd); 
		return -3;     
	} 
	//mask
	sin->sin_addr.s_addr = network_param.m_mask;
	if( ioctl( sockfd, SIOCSIFNETMASK, &ifr_ip) < 0 )     
	{   
		close(sockfd); 
		return -4;     
	}
	//gateway
	ret =set_dsthost_gateway(network_param.m_gateway, dest_host, interface_name);
	D_PRT("%s->%d:ret:%d\n", __FILE__, __LINE__, ret);
	//dns

	//设置激活标志  
	ifr_ip.ifr_flags |= IFF_UP |IFF_RUNNING;  

	//get the status of the device  
	if( ioctl( sockfd, SIOCSIFFLAGS, &ifr_ip ) < 0 )  
	{  
		return -7;  
	}

	return 1;
}
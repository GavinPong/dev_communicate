#ifndef __NETWORK_TOOLS_H__
#define __NETWORK_TOOLS_H__

#ifdef __cplusplus
extern"C"{
#endif

typedef struct _network_param_s{
	char m_en_dhcp;
	unsigned long m_ip;
	unsigned long m_mask;
	unsigned long m_gateway;
	unsigned long m_dns;
}network_param_t;

int str_to_ip(char *pIpaddr);
void ip_to_str(unsigned long dwIp, char *pIpaddr);
int get_dsthost_gateway(unsigned long *gateway, unsigned int dest_host, char *interface_name);
int set_dsthost_gateway(unsigned long gateway, unsigned int dest_host, char *interface_name);
int get_dns(unsigned long *dns1, unsigned long *dns2);
int get_network(network_param_t *pnetwork_param, char *interface_name);
int set_network(network_param_t network_param, char *interface_name);

#ifdef __cplusplus
}
#endif

#endif
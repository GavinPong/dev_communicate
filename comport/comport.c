#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "comport.h"

typedef struct _comport_list_s{
	struct _comport_list_s *m_next, *m_prev;
}comport_list_t;

#define  COMPORTHANDLE_2_COMPORTDATA(m)  ((comport_data_t *)m)

typedef struct _comport_data_s{
	int m_fd;					//设备句柄,必须是第一个成员，方便快速转换成结构体指针
	comport_list_t m_list;		//链表索引
	char m_name[16];			//设备对应的驱动名
	comport_cfg_t m_cfg;		//串口配置
}comport_data_t;

typedef struct _comport_ctx_s{
	comport_list_t m_list_head;
	int m_inited;
}comport_ctx_t;

//_======变量区=======
comport_ctx_t s_comport_ctx;
//_==================

//_注意链表头没有关联任何数据，所以链表头始终不能动
static void comport_list_init_listhead(comport_list_t *list_head){
	list_head->m_next = list_head;
	list_head->m_prev = list_head;
}

static void comport_list_add(comport_list_t *cur, comport_list_t *prev, comport_list_t *next){
	cur->m_prev = prev;
	cur->m_next = next;
	next->m_prev = cur;
	prev->m_next = cur;
}

//_加到链表尾巴，相当于加在链表头和链表尾之间,注意链表头没有关联任何数据，所以立案表头始终不能动
static void comport_list_add_to_tail(comport_list_t *list_head, comport_list_t *sub_list){
	comport_list_add(sub_list, list_head->m_next, list_head);
}

//_加到链表头部，相当于加在链表头和链表尾之间,注意链表头没有关联任何数据，所以立案表头始终不能动
static void comport_list_add_to_head(comport_list_t *list_head, comport_list_t *sub_list){
	comport_list_add(sub_list, list_head, list_head->m_next);
}

static void _comport_list_del(comport_list_t *prev, comport_list_t *next){
	prev->m_next = next;
	next->m_prev = prev;
}

static void comport_list_del(comport_list_t *cur){
	_comport_list_del(cur->m_prev, cur->m_next);
	cur->m_prev = NULL;
	cur->m_next = NULL;
}

//_头结点的上一个节点指针 和 下一个节点指针都指向头结点本身，代表为empty
static int comport_list_is_empty(comport_list_t *head){
	comport_list_t *next = head->m_next;

	return (next == head) && (next == head->m_prev);
}

static comport_list_t *comport_list_get_tail(comport_list_t *head){
	return head->m_prev;
}

#define com_container_of(ptr, type, member)  (type *)((char *)ptr - offsetof(type, member))

#define com_list_get_entry(ptr, type, member)	\
	com_container_of(ptr, type, member)

#define com_list_for_each(pos, n, head)	\
	for ((pos) = (head)->m_next, n = (pos)->m_next; (pos) != (head); (pos) = n, n = (pos)->m_next)

#define com_list_for_each_prev(pos, n, head)	\
	for((pos) = (head)->m_prev, n = (pos)->m_prev; (pos) != (head); (pos) = n, n = (pos)->m_prev)

//正向遍历利用head，和data中包含的member完成遍历
#define com_list_for_each_entry(type, pos, n, head, member)	\
	for((pos) = com_list_get_entry((head)->m_next, type, member), (n) = com_list_get_entry((pos)->member.m_next, type, member);	\
		&(pos)->member != (head);	\
		pos = n, (n) = com_list_get_entry((pos)->member.m_next, type, member)\
	)

//反向遍历，利用head，和data中包含的member完成遍历 
#define com_list_for_each_entry_prev(type, pos, n, head, member)	\
	for((pos) = com_list_get_entry((head)->m_prev, type, member), (n) = com_list_get_entry((pos)->member.m_prev, type, member);	\
	&(pos)->member != (head);	\
	(pos) = (n), (n) = com_list_get_entry((pos)->member.m_prev, type, member)\
	)

int comport_set_dev_attr(comport_data_t *comport_data){
	if (!s_comport_ctx.m_inited)
	{
		return COMPORT_ERR_UNINIT;
	}
	if(!comport_data)
		return COMPORT_ERR_PARAM;
	int speer_arr[] = {B110, B300, B600, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B921600};
	struct termios com_opt;
	
	if(tcgetattr(comport_data->m_fd, &com_opt)){
		return COMPORT_FALSE;
	}
	//停止位
	switch(comport_data->m_cfg.m_stop_bit){
		case STOPBITS_1:
				com_opt.c_cflag &= ~CSTOPB;
			break;
		case STOPBITS_2:
				com_opt.c_cflag |= ~CSTOPB;
			break;
		default:
				com_opt.c_cflag &= ~CSTOPB;
			break;
	}
	//数据位
	com_opt.c_cflag &= ~CSIZE;//清空数据位设置
	//根据配置从新设置数据位
	switch(comport_data->m_cfg.m_data_bit){
	case DATA_BIT_1:
		break;
	case DATA_BIT_2:
		break;
	case DATA_BIT_3:
		break;
	case DATA_BIT_4:
		break;
	case DATA_BIT_5:
		com_opt.c_cflag |= CS5;
		break;
	case DATA_BIT_6:
		com_opt.c_cflag |= CS6;
		break;
	case DATA_BIT_7:
		com_opt.c_cflag |= CS7;
		break;
	case DATA_BIT_8:
		com_opt.c_cflag |= CS8;
		break;
	default:
		com_opt.c_cflag |= CS8;
		break;
	}
	//校验位
	switch(comport_data->m_cfg.m_parity){
	case PARITY_EVEN:
		com_opt.c_cflag |= PARENB;        //enable parity         
		com_opt.c_cflag &= ~PARODD;       //偶校验         
		com_opt.c_iflag |= INPCK;         //disable pairty checking
		break;
	case PARITY_ODD:
		com_opt.c_cflag |= PARENB;        //enable parity
		com_opt.c_cflag |= PARODD;        //奇校验
		com_opt.c_iflag |= INPCK;         //disable parity checking
		 break;
	case PARITY_NONE:
		com_opt.c_cflag &= ~PARENB;       //清除校验位
		com_opt.c_iflag &= ~INPCK;        //enable parity checking
		break;
	default:
		break;
	}
	com_opt.c_cflag |= (CLOCAL | CREAD);     
	com_opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); //禁用echo       
	com_opt.c_oflag &= ~OPOST;     
	com_opt.c_oflag &= ~(ONLCR | OCRNL);    //添加的        
	com_opt.c_iflag &= ~(ICRNL | INLCR);    //添加的no parity             
	com_opt.c_iflag &= ~(IXON | IXOFF | IXANY);    //添加的     
	com_opt.c_cc[VTIME] = 0;        //设置超时为15sec     
	com_opt.c_cc[VMIN] = 0;        //Update the Opt and do it now     
	tcflush(comport_data->m_fd, TCIFLUSH);     
	//波特率
	cfsetispeed(&com_opt, speer_arr[comport_data->m_cfg.m_baud_rate]);
	cfsetospeed(&com_opt, speer_arr[comport_data->m_cfg.m_baud_rate]);
	if (tcsetattr(comport_data->m_fd, TCSANOW, &com_opt) != 0)
	{
		return COMPORT_ERR_SET_ATTR;
	}
	
	return COMPORT_OK;
}

//cfg:只负责运行时的配置，配置的保存应该由上层自己完成
int comport_init_cfg(comport_cfg_t *comport_cfg){
	if (!comport_cfg)
	{
		return COMPORT_ERR_PARAM;
	}
	if (!s_comport_ctx.m_inited)
	{
		return COMPORT_ERR_UNINIT;
	}
	comport_cfg->m_baud_rate = BAUD_RATE_9600;
	comport_cfg->m_data_bit = DATA_BIT_8;
	comport_cfg->m_flow_control = FLOW_CONTROL_NONE;
	comport_cfg->m_parity = PARITY_NONE;
	comport_cfg->m_stop_bit = STOPBITS_1;
	return COMPORT_OK;
}

int comport_uninit_cfg(comport_data_t *comport_data)
{
	if (!s_comport_ctx.m_inited)
	{
		return COMPORT_ERR_UNINIT;
	}

	return COMPORT_OK;
}

int comport_get_cfg(comport_handle_t comport_handle, comport_cfg_t *comport_cfg)
{
	if (comport_handle <= 0 || !comport_cfg)
	{
		return COMPORT_ERR_PARAM;
	}
	if (!s_comport_ctx.m_inited)
	{
		return COMPORT_ERR_UNINIT;
	}
	comport_data_t *p_comport_data = COMPORTHANDLE_2_COMPORTDATA(comport_handle);
	*comport_cfg = p_comport_data->m_cfg;

	return COMPORT_OK;
}

int comport_set_cfg(comport_handle_t comport_handle, comport_cfg_t comport_cfg)
{
	if (comport_handle <= 0)
	{
		return COMPORT_ERR_PARAM;
	}
	if (!s_comport_ctx.m_inited)
	{
		return COMPORT_ERR_UNINIT;
	}
	comport_data_t *p_comport_data = COMPORTHANDLE_2_COMPORTDATA(comport_handle);
	comport_cfg_t cfg_tmp = p_comport_data->m_cfg;
	p_comport_data->m_cfg = comport_cfg;
	if(COMPORT_OK != comport_set_dev_attr(p_comport_data))
	{
		p_comport_data->m_cfg = cfg_tmp;
		comport_set_dev_attr(p_comport_data);
		return COMPORT_FALSE;
	}
	return COMPORT_OK;
}

//return value > 0:接收的字节数   < 0 失败，具体建错误值列表
int comport_transfer_recv(comport_handle_t comport_handle, char *buf, int buf_size, int milli_sec)
{
	if (comport_handle <= 0)
	{
		printf("%s->%d\n", __FILE__, __LINE__);
		return COMPORT_ERR_PARAM;
	}
	if (!s_comport_ctx.m_inited)
	{
		printf("%s->%d\n", __FILE__, __LINE__);
		return COMPORT_ERR_UNINIT;
	}
	comport_data_t *p_comport_data = COMPORTHANDLE_2_COMPORTDATA(comport_handle);
	struct timeval timeout;
	fd_set rset;
	int nfd_max = p_comport_data->m_fd + 1;
	int ret = -1;

	timeout.tv_sec = milli_sec/1000;
	timeout.tv_usec = (milli_sec % 1000) * 1000;
	FD_ZERO(&rset);
	FD_SET(p_comport_data->m_fd, &rset);
	ret = select(nfd_max, &rset, NULL,NULL, &timeout);
	if (0 == ret)
	{
		//printf("%s->%d\n", __FILE__, __LINE__);
		return COMPORT_ERR_TIMEOUT;
	}
	else if (ret < 0)
	{
		//printf("%s->%d\n", __FILE__, __LINE__);
		return COMPORT_ERR_RECV;
	}
#if 1
	int ret1 = read(p_comport_data->m_fd, buf, buf_size);
	printf("%s->%d:read count:%d\n", __FILE__, __LINE__, ret1);
	return ret1;
#endif
	return read(p_comport_data->m_fd, buf, buf_size);
}

//return value > 0:发送的字节数   < 0 失败，具体建错误值列表
int comport_transfer_send(comport_handle_t comport_handle, const char *buf, int buf_size)
{
	if (!s_comport_ctx.m_inited)
	{
		return COMPORT_ERR_UNINIT;
	}
	if (comport_handle <= 0)
	{
		return COMPORT_ERR_PARAM;
	}
	comport_data_t *p_comport_data = COMPORTHANDLE_2_COMPORTDATA(comport_handle);
	
	return write(p_comport_data->m_fd, buf, buf_size);
}

int comport_module_init(){
	comport_list_init_listhead(&s_comport_ctx.m_list_head);
	s_comport_ctx.m_inited = 1;

	return COMPORT_OK;
}

int comport_module_uninit(){
	s_comport_ctx.m_inited = 0;
	comport_data_t *pos = NULL, *n = NULL;
	comport_list_t *head = &s_comport_ctx.m_list_head;
	com_list_for_each_entry(comport_data_t, pos, n, head, m_list){
		close(pos->m_fd);
		comport_list_del(&pos->m_list);
		free(pos);
	}

	return COMPORT_OK;
}

int comport_open(comport_handle_t *comport_handle, char *devname)
{
	if (!comport_handle || !devname)
	{
		return COMPORT_ERR_PARAM;
	}
	*comport_handle = NULL;
	if (!s_comport_ctx.m_inited)
	{
		return COMPORT_ERR_UNINIT;
	}
	comport_data_t *pos = NULL, *n = NULL;
	comport_list_t *head = &s_comport_ctx.m_list_head;

	com_list_for_each_entry(comport_data_t, pos, n, head, m_list){
		if(!strcmp(pos->m_name, devname))
		{
			return COMPORT_ERR_BUSY;
		}
	}
	int fd = open(devname, O_RDWR | O_NOCTTY);
	if (fd < 0)
	{
		return COMPORT_ERR_OPEN;
	}
	comport_data_t *p_comport_data = (comport_data_t *)calloc(1, sizeof(comport_data_t));
	if (!p_comport_data)
	{
		close(fd);
		return COMPORT_ERR_NOMEMORY;
	}
	p_comport_data->m_fd = fd;
	strncpy(p_comport_data->m_name, devname, sizeof(p_comport_data->m_name));
	comport_init_cfg(&p_comport_data->m_cfg);
	if(COMPORT_OK != comport_set_dev_attr(p_comport_data))
	{
		free(p_comport_data);
		return COMPORT_FALSE;
	}
	comport_list_add_to_tail(&s_comport_ctx.m_list_head, &p_comport_data->m_list);
	*comport_handle = &p_comport_data->m_fd;

	return COMPORT_OK;
}

int comport_close(comport_handle_t comport_handle)
{
	if (!s_comport_ctx.m_inited)
	{
		return COMPORT_ERR_UNINIT;
	}
	comport_data_t *p_comport_data = COMPORTHANDLE_2_COMPORTDATA(comport_handle);
	close(p_comport_data->m_fd);
	comport_list_del(&p_comport_data->m_list);
	free(p_comport_data);

	return COMPORT_OK;
}
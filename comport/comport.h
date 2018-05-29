#ifndef __COMPORT_H__
#define __COMPORT_H__

#ifdef _cplusplus
extern"C"{
#endif

//return  value
#define COMPORT_OK 1					
#define COMPORT_FALSE 0
#define COMPORT_ERR_OPEN -1			//�豸��ʧ��
#define COMPORT_ERR_SEND -2			//����ʧ��
#define COMPORT_ERR_RECV -3			//����ʧ��
#define COMPORT_ERR_NOMEMORY -4		//�ڴ治��
#define COMPORT_ERR_PARAM -5		//�����쳣
#define COMPORT_ERR_UNINIT -6		//û�г�ʼ��
#define COMPORT_ERR_BUSY -7			//�豸��ռ��
#define COMPORT_ERR_SET_ATTR -8		//��������ʧ��
#define COMPORT_ERR_TIMEOUT -9		//��ʱ

typedef void*  comport_handle_t;	//*handle ����ֱ�Ӽ��ص��ϲ��select�м���

typedef enum _com_baud_rate_e{
	BAUD_RATE_110 = 0,
	BAUD_RATE_300,
	BAUD_RATE_600,
	BAUD_RATE_2400,
	BAUD_RATE_4800,
	BAUD_RATE_9600,
	BAUD_RATE_19200,
	BAUD_RATE_38400,
	BAUD_RATE_57600,
	BAUD_RATE_115200,
	BAUD_RATE_230400,
	BAUD_RATE_460800,
	BAUD_RATE_921600,
}com_baud_rate_e;

typedef enum _com_data_bit_e{
	DATA_BIT_1 = 0,
	DATA_BIT_2,
	DATA_BIT_3,
	DATA_BIT_4,
	DATA_BIT_5,
	DATA_BIT_6,
	DATA_BIT_7,
	DATA_BIT_8,
}com_data_bit_e;

typedef enum _com_parity_e{
	PARITY_EVEN = 0,
	PARITY_ODD,
	PARITY_NONE,
}com_parity_e;

typedef enum _com_stop_bit_e{
	STOPBITS_1 = 0,
	STOPBITS_2,
}com_stop_bit_e;

typedef enum _com_flow_control_e{
	FLOW_CONTROL_XON_XOFF = 0,
	FLOW_CONTROL_HARDWARE,
	FLOW_CONTROL_NONE,
}com_flow_control_e;

typedef struct _comport_cfg_s{
	com_baud_rate_e m_baud_rate;		//������
	com_data_bit_e m_data_bit;			//����λ
	com_parity_e m_parity;				//��żУ��λ
	com_stop_bit_e m_stop_bit;			//ֹͣλ
	com_flow_control_e m_flow_control;	//����
}comport_cfg_t;

int comport_module_init();
int comport_module_uninit();
int comport_open(comport_handle_t *comport_handle, char *devname);
int comport_close(comport_handle_t comport_handle);

//cfg:ֻ��������ʱ�����ã����õı���Ӧ�����ϲ��Լ����
int comport_set_cfg(comport_handle_t comport_handle, comport_cfg_t comport_cfg);
int comport_get_cfg(comport_handle_t comport_handle, comport_cfg_t *comport_cfg);
//data transfer
int comport_transfer_recv(comport_handle_t comport_handle, char *buf, int buf_size, int milli_sec);
int comport_transfer_send(comport_handle_t comport_handle, const char *buf, int buf_size);


#ifdef _cplusplus
}
#endif

#endif
#ifndef __COMPORT_H__
#define __COMPORT_H__

#ifdef _cplusplus
extern"C"{
#endif

//return  value
#define COMPORT_OK 1					
#define COMPORT_FALSE 0
#define COMPORT_ERR_OPEN -1			//设备打开失败
#define COMPORT_ERR_SEND -2			//发送失败
#define COMPORT_ERR_RECV -3			//接收失败
#define COMPORT_ERR_NOMEMORY -4		//内存不足
#define COMPORT_ERR_PARAM -5		//参数异常
#define COMPORT_ERR_UNINIT -6		//没有初始化
#define COMPORT_ERR_BUSY -7			//设备被占用
#define COMPORT_ERR_SET_ATTR -8		//设置属性失败
#define COMPORT_ERR_TIMEOUT -9		//超时

typedef void*  comport_handle_t;	//*handle 可以直接加载到上层的select中监听

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
	com_baud_rate_e m_baud_rate;		//波特率
	com_data_bit_e m_data_bit;			//数据位
	com_parity_e m_parity;				//奇偶校验位
	com_stop_bit_e m_stop_bit;			//停止位
	com_flow_control_e m_flow_control;	//流控
}comport_cfg_t;

int comport_module_init();
int comport_module_uninit();
int comport_open(comport_handle_t *comport_handle, char *devname);
int comport_close(comport_handle_t comport_handle);

//cfg:只负责运行时的配置，配置的保存应该由上层自己完成
int comport_set_cfg(comport_handle_t comport_handle, comport_cfg_t comport_cfg);
int comport_get_cfg(comport_handle_t comport_handle, comport_cfg_t *comport_cfg);
//data transfer
int comport_transfer_recv(comport_handle_t comport_handle, char *buf, int buf_size, int milli_sec);
int comport_transfer_send(comport_handle_t comport_handle, const char *buf, int buf_size);


#ifdef _cplusplus
}
#endif

#endif
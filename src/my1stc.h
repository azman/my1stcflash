/*----------------------------------------------------------------------------*/
#ifndef __MY1STCH__
#define __MY1STCH__
/*----------------------------------------------------------------------------*/
#include "my1uart.h"
/*----------------------------------------------------------------------------*/
#define STC_SYNC_CHAR 0x7f
#define STC_SYNC_TIMEOUT_US 200000
#define STC_SYNC_INIT -2
#define STC_SYNC_MISS -1
#define STC_SYNC_DONE 0
#define STC_SYNC_NONE 1
#define STC_SYNC_REST 2
/*----------------------------------------------------------------------------*/
#define STC_PACKET_SIZE 65536
/* packet buff size is minus the first 2-byte start headers */
#define STC_PACKET_BUFF_SIZE (STC_PACKET_SIZE-2)
/* max data length is actually 0xffff (65535) - dflag,dsize,cksum,emark (6) */
#define STC_MAX_DATA_LEN 65535-6
/*----------------------------------------------------------------------------*/
#define STC_PACKET_M0 0x46
#define STC_PACKET_M1 0xb9
#define STC_PACKET_MX ((STC_PACKET_M0<<8)|STC_PACKET_M1)
#define STC_PACKET_ME 0x16
#define STC_PACKET_HOST2MCU 0x6a
#define STC_PACKET_MCU2HOST 0x68
#define STC_PACKET_DATA_OFFSET 5
/*----------------------------------------------------------------------------*/
#define STC_PACKET_VALID 0
#define STC_PACKET_USER_ABORT -1
#define STC_PACKET_ERROR_BEGMARK0 -2
#define STC_PACKET_ERROR_BEGMARK1 -3
#define STC_PACKET_ERROR_ENDMARK -4
#define STC_PACKET_ERROR_DIRECT -5
#define STC_PACKET_ERROR_LENGTH -6
#define STC_PACKET_ERROR_CHECKSUM -7
#define STC_PACKET_ERROR_OVERFLOW -8
/*----------------------------------------------------------------------------*/
#define STC_PACKET_HANDSHAKE_ERROR -10
#define STC_PACKET_FLASH_ERROR -20
#define STC_PACKET_BAUDDANCE_ERROR -30
#define STC_PACKET_BAUDRATE_ERROR -40
#define STC_PACKET_BAUDPONG_ERROR -50
/*----------------------------------------------------------------------------*/
typedef struct _stc_packet_t
{
	unsigned short imark; /* init marker 0x46,0xb9 -> big endian U+46b9 */
	unsigned short psize; /* big-endian 16-bit length (excluding m0,m1) */
	unsigned char* pdata; /* data pointer */
	unsigned short dsize; /* data size */
	unsigned short cksum; /* big-endian 16-bit modular sum: dir+len+data */
	unsigned char hflag; /* 0x6a = host2mcu, 0x68 = all_other */
	unsigned char emark; /* end marker 0x16 */
}
stc_packet_t;
/*----------------------------------------------------------------------------*/
typedef struct _stc_payload_info_t
{
	unsigned char flag; /* 0x50 */
	unsigned short sync[8]; /* 16-bit big-endian */
	unsigned char ver1; /* bcd format? */
	unsigned char ver2; /* single ascii char? */
	unsigned char nul1; /* nul-byte */
	unsigned char mid[2]; /* 2-byte model id! */
	unsigned char nul2; /* 0x8c? */
	unsigned char ms[8]; /* options??? */
	unsigned char uid[7]; /* 7-bytes unique id? */
	unsigned char un1[5]; /* unknown bytes */
	/* total 43 bytes??? */
} __attribute__ ((packed))
stc_payload_info_t;
/*----------------------------------------------------------------------------*/
#define PAYLOAD_INFO_ID 0x50
#define PAYLOAD_INFO_OFFSET_FLAG 0x00
#define PAYLOAD_INFO_OFFSET_SYNC 0x01
#define PAYLOAD_INFO_OFFSET_VER1 0x11
#define PAYLOAD_INFO_OFFSET_VER2 0x12
#define PAYLOAD_INFO_OFFSET_MID1 0x14
#define PAYLOAD_INFO_OFFSET_MID2 0x15
#define PAYLOAD_HANDSHAKE_ID 0x8f
#define PAYLOAD_BAUD_CONFIRM 0x8e
#define PAYLOAD_ERASE_MEMORY 0x84
#define PAYLOAD_FLASH_MEMORY 0x00
#define PAYLOAD_FLASH_FINISH 0x69
#define PAYLOAD_FLASH_OPTION 0x8d
#define PAYLOAD_DEVICE_RESET 0x82
#define PAYLOAD_BAUD_CHKPONG 0x80
/*----------------------------------------------------------------------------*/
#define STC_DEVICE_NAME_LEN 16
#define STC_FLASH_BLOCK_SIZE 128
#define STC_FLASH_BLOCK_SIZE_PHYSICAL 512
/*----------------------------------------------------------------------------*/
typedef struct _stc_dev_t
{
	int timeout_us, error, baudhand, baudrate, baud_err;
	int pcount;
	unsigned char packet[STC_PACKET_SIZE];
	stc_packet_t info;
	int flag, uid0, uid1, csum;
	char label[STC_DEVICE_NAME_LEN];
	int fw11, fw12, fw20;
	int fmemsize, ememsize; /* flash size & eeprom size */
	float freq;
	unsigned char opts[4]; /* for stc12c5a60s2 */
	/* flash data? */
	int datasize;
	unsigned char *data;
}
stc_dev_t;
/*----------------------------------------------------------------------------*/
/** mcu-id1, mcu-id2, mcu-name, mcu-flash-size (kB), mcu-eeprom-size (kB) */
typedef struct _stcmcu_t
{
	int uid0, uid1;
	char label[STC_DEVICE_NAME_LEN];
	int fmsz, emsz, flag;
}
stcmcu_t;
/*----------------------------------------------------------------------------*/
int stc_check_isp(stc_dev_t* pdevice, my1uart_t* pport);
int stc_handshake(stc_dev_t* pdevice, my1uart_t* pport);
int stc_bauddance(stc_dev_t* pdevice, my1uart_t* pport);
int stc_baud_pong(stc_dev_t* pdevice, my1uart_t* pport);
int stc_erase_mem(stc_dev_t* pdevice, my1uart_t* pport);
int stc_flash_mem(stc_dev_t* pdevice, my1uart_t* pport);
int stc_send_opts(stc_dev_t* pdevice, my1uart_t* pport);
int stc_reset_dev(stc_dev_t* pdevice, my1uart_t* pport);
/*----------------------------------------------------------------------------*/
#endif
/*----------------------------------------------------------------------------*/

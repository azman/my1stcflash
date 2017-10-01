/*----------------------------------------------------------------------------*/
#ifndef __MY1STCH__
#define __MY1STCH__
/*----------------------------------------------------------------------------*/
#include "my1comlib.h"
/*----------------------------------------------------------------------------*/
#define STC_SYNC_CHAR 0x7f
#define STC_SYNC_TIMEOUT_US 150000
#define STC_SYNC_INIT -2
#define STC_SYNC_MISS -1
#define STC_SYNC_DONE 0
#define STC_SYNC_NONE 1
#define STC_SYNC_REST 2
/*----------------------------------------------------------------------------*/
#define STC_PACKET_SIZE 65536
/* max data length is actually 0xffff (65535) - dflag,dsize,cksum,emark (6) */
#define STC_MAX_DATA_LEN 65535-6
/*----------------------------------------------------------------------------*/
#define STC_PACKET_MX 0x46b9
#define STC_PACKET_M0 0x46
#define STC_PACKET_M1 0xb9
#define STC_PACKET_ME 0x16
#define STC_PACKET_HOST2MCU 0x6a
#define STC_PACKET_MCU2HOST 0x68
#define STC_PACKET_DATA_OFFSET 5
/*----------------------------------------------------------------------------*/
#define STC_PACKET_VALID 0
#define STC_PACKET_ERROR_BEGMARK 1
#define STC_PACKET_ERROR_ENDMARK 2
#define STC_PACKET_ERROR_DIRECT 3
#define STC_PACKET_ERROR_LENGTH 4
#define STC_PACKET_ERROR_CHECKSUM 5
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
	unsigned char ms[8]; /* what??? */
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
/*----------------------------------------------------------------------------*/
#define STC_DEVICE_12C5A60S2_MID1 0xd1
#define STC_DEVICE_12C5A60S2_MID2 0x7e
/*----------------------------------------------------------------------------*/
typedef struct _stc_dev_t
{
	int timeout_us, error;
	int pcount;
	unsigned char packet[STC_PACKET_SIZE];
	stc_packet_t info;
	int flag, uid0, uid1;
	int fw11, fw12, fw20;
	float freq;
}
stc_dev_t;
/*----------------------------------------------------------------------------*/
int stc_check_isp(stc_dev_t* pdevice, serial_port_t* pport);
int stc_check_info(stc_dev_t* pdevice);
int stc_handshake(stc_dev_t* pdevice, serial_port_t* pport);
int stc_bauddance(stc_dev_t* pdevice, serial_port_t* pport);
/*----------------------------------------------------------------------------*/
#endif
/*----------------------------------------------------------------------------*/

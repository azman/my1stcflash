/*----------------------------------------------------------------------------*/
#ifndef __MY1STCH__
#define __MY1STCH__
/*----------------------------------------------------------------------------*/
#include "my1comlib.h"
/*----------------------------------------------------------------------------*/
#define STC_SYNC_CHAR 0x7f
#define STC_SYNC_T_MS 150 
#define STC_SYNC_INIT -2
#define STC_SYNC_MISS -1
#define STC_SYNC_DONE 0
#define STC_SYNC_NONE 1
#define STC_SYNC_REST 2
/*----------------------------------------------------------------------------*/
/*
Basic frame format
------------------

M0 M1 DR L0 L1 D0 ... Dn C0 C1 ME

M0 := 0x46
M1 := 0xb9
DR := 0x6a if host2mcu else 0x68
L  := 16 bit big endian packet length, counted from DR to ME
C  := 16 big endian modular sum from DR to Dn
ME := 0x16

D0..Dn is the packet payload

In most cases, the first byte of the payload marks the type of packet
or type of command. Responses by the MCU often use this type to tell
the programmer software which kind of command should follow. For
instance, after the baudrate handshake, the MCU replies with a
type 0x84 packet, and 0x84 is used for "erase" command packets from
the host.

Fun fact: The start marker (0x46, 0xb9) interpreted as UTF-16 is the
Unicode character U+46B9, which is an unusual CJK ideograph (䚹)
which translates as "to prepare" or "all ready" into English. How
fitting! This might not be a coincidence.
*/
/*----------------------------------------------------------------------------*/
#define STC_PACKET_SIZE 65536
/* max data length is actually 0xffff (65535) - dflag,dsize,cksum,emark (6) */
#define STC_MAX_DATA_LEN 65535-6
/*----------------------------------------------------------------------------*/
#define STC_PACKET_MX 0x46b9
#define STC_PACKET_ME 0x16
#define STC_PACKET_HOST2MCU 0x6a
#define STC_PACKET_MCU2HOST 0x68
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
	unsigned char hflag; /* 0x6a = host2mcu, 0x68 = all_other */
	unsigned short dsize; /* big-endian 16-bit length (excluding m0,m1) */
	unsigned char* pdata;
	unsigned short psize;
	unsigned short cksum; /* big-endian 16-bit modular sum: dir+len+data */
	unsigned char emark; /* end marker 0x16 */
}
stc_packet_t;
/*----------------------------------------------------------------------------*/
/*
Info I got are from stcgal https://github.com/grigorig/stcgal (doc)

Payload: 0x50, SYNC00, SYNC01, ..., SYNC70, SYNC71,
         V1, V2, 0x00, ID0, ID1, 0x8c,
         MS0, ..., MS7,
         UID0, ..., UID6,
         unknown bytes follow
SYNC* := sequence of 8 16-bit big-endian counter values, recorded
         from the initial 0x7f sync sequence. this can be used to
         determine the MCU clock frequency.

V1    := version number, two digits packed BCD.
V2    := stepping, one ASCII character.
ID0   := MCU model ID, byte 1
ID1   := MCU model ID, byte 2
UID0...UID6 := 7 bytes of unique id

UID is only sent by some BSL versions, others send zero bytes.
*/
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
}
stc_payload_info_t;
#define PAYLOAD_INFO_ID 0x50
#define PAYLOAD_INFO_OFFSET_FLAG 0x00
#define PAYLOAD_INFO_OFFSET_SYNC 0x09
#define PAYLOAD_INFO_OFFSET_VER1 0x11
#define PAYLOAD_INFO_OFFSET_VER2 0x12
/*----------------------------------------------------------------------------*/
typedef struct _stc_dev_t
{
	int pcount;
	unsigned char packet[STC_PACKET_SIZE]; /* raw packet, no end marker */
	stc_packet_t packinfo;
}
stc_dev_t;
/*----------------------------------------------------------------------------*/
int stc_check_isp(stc_dev_t* pdevice, serial_port_t* pport, int timeout_ms);
int stc_validate_packet(stc_dev_t* pdevice);
/*----------------------------------------------------------------------------*/
#endif
/*----------------------------------------------------------------------------*/

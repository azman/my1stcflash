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

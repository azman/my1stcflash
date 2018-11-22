/*----------------------------------------------------------------------------*/
#include "my1stc.h"
#include "my1cons.h"
#include <sys/time.h>
/*----------------------------------------------------------------------------*/
int stc_wait_packet(stc_dev_t* pdevice, serial_port_t* pport)
{
	int temp,size=0;
	pdevice->pcount = 0;
	while(1)
	{
		temp = get_byte_serial_timed(pport,pdevice->timeout_us);
		if(temp==SERIAL_TIMEOUT)
			break;
		if(pdevice->pcount<STC_PACKET_SIZE)
			pdevice->packet[pdevice->pcount++] = (byte_t) temp;
		else
			break;
		if (pdevice->pcount>5)
		{
			if ((pdevice->pcount>=size+2)&&temp==STC_PACKET_ME) break;
		}
		else if (pdevice->pcount==5)
		{
			size = ((int)pdevice->packet[3]<<8) + pdevice->packet[4];
		}
	}
	return temp==SERIAL_TIMEOUT?temp:pdevice->pcount;
}
/*----------------------------------------------------------------------------*/
unsigned short stc_generate_chksum(stc_dev_t* pdevice)
{
	int loop;
	unsigned short csum = 0x0000;
	for(loop=2;loop<pdevice->pcount-3;loop++)
		csum += pdevice->packet[loop];
	return csum;
}
/*----------------------------------------------------------------------------*/
int stc_validate_packet(stc_dev_t* pdevice)
{
	int test = STC_PACKET_VALID, loop;
	/* 16-bit big-endian init marker */
	pdevice->info.imark = ((int)pdevice->packet[0]<<8) + pdevice->packet[1];
	/* direction */
	pdevice->info.hflag = (int)pdevice->packet[2];
	/* 16-bit big-endian length */
	pdevice->info.psize = ((int)pdevice->packet[3]<<8) + pdevice->packet[4];
	/* assign data pointer */
	pdevice->info.pdata = &pdevice->packet[5];
	pdevice->info.dsize = pdevice->info.psize - 6;
	/* check packet size for 'implied' end marker? */
	loop = pdevice->pcount;
	if (loop==STC_PACKET_SIZE)
	{
		/* end marker is implied */
		pdevice->info.emark = STC_PACKET_ME;
	}
	else
	{
		loop--;
		pdevice->info.emark = pdevice->packet[loop];
	}
	/* 16-bit big-endian checksum */
	pdevice->info.cksum = ((int)pdevice->packet[loop-2]<<8) +
		pdevice->packet[loop-1];
	pdevice->csum = stc_generate_chksum(pdevice);
	/* do validation! */
	if (pdevice->info.imark!=STC_PACKET_MX)
		test = STC_PACKET_ERROR_BEGMARK;
	else if (pdevice->info.hflag!=STC_PACKET_MCU2HOST)
		test = STC_PACKET_ERROR_DIRECT;
	else if (pdevice->info.psize!=pdevice->pcount-2)
		test = STC_PACKET_ERROR_LENGTH;
	else if (pdevice->info.emark!=STC_PACKET_ME)
		test = STC_PACKET_ERROR_ENDMARK;
	else if (pdevice->info.cksum!=pdevice->csum)
		test = STC_PACKET_ERROR_CHECKSUM;
	/* update error flag */
	pdevice->error += test;
	return test;
}
/*----------------------------------------------------------------------------*/
unsigned short change_endian(unsigned short test)
{
	unsigned char *byte = (unsigned char*) &test;
	unsigned short swap = byte[0];
	return (swap<<8)|byte[1];
}
/*----------------------------------------------------------------------------*/
int stc_check_isp(stc_dev_t* pdevice, serial_port_t* pport)
{
	struct timeval inittime, currtime;
	my1key_t key;
	int do_wait = 0, loop;
	int status = STC_SYNC_INIT;
	while(status<STC_SYNC_DONE)
	{
		if(!do_wait)
		{
			put_byte_serial(pport,STC_SYNC_CHAR);
			gettimeofday(&inittime,0x0);
			do_wait = 1;
		}
		else
		{
			/* gettimeofday is using micro-second resolution! */
			gettimeofday(&currtime,0x0);
			if((currtime.tv_sec>inittime.tv_sec)||
				(currtime.tv_usec-inittime.tv_usec)>pdevice->timeout_us)
			{
				do_wait = 0;
			}
		}
		if(check_incoming(pport))
		{
			if (stc_wait_packet(pdevice,pport)==SERIAL_TIMEOUT)
				status = STC_SYNC_MISS;
			else if (stc_validate_packet(pdevice)!=STC_PACKET_VALID)
				status = STC_SYNC_VERR;
			else
			{
				unsigned long test = 0;
				stc_payload_info_t *info =
					(stc_payload_info_t*) pdevice->info.pdata;
				pdevice->flag = info->flag;
				/* detect target frequency */
				for (loop=0;loop<8;loop++)
					test += (unsigned long)change_endian(info->sync[loop]);
				test >>= 3; /* divide-by-8: get average */
				test *= 1200; /* target minimum baudrate? */
				test /= 5816; /* dunno why? */
				if ((test%100)>=50) test = (test/100)+1;
				else test /= 100;
				switch (test)
				{
					case 24:
						pdevice->freq = 24.000000; /* 24 mhz? */
						break;
					case 22:
						pdevice->freq = 22.118400;
						break;
					case 16:
						pdevice->freq = 16.000000;
						break;
					default:
					case 12:
						pdevice->freq = 12.000000;
						break;
					case 11:
						pdevice->freq = 11.059200;
						break;
					case 8:
						pdevice->freq = 8.000000;
						break;
					case 6:
						pdevice->freq = 6.000000;
						break;
					case 4:
						pdevice->freq = 4.000000;
						break;
				}
				/* other stuff */
				pdevice->uid0 = info->mid[0];
				pdevice->uid1 = info->mid[1];
				pdevice->fw11 = info->ver1;
				pdevice->fw12 = (pdevice->fw11&0x0f);
				pdevice->fw11 = (pdevice->fw11&0xf0)>>4;
				pdevice->fw20 = info->ver2;
				status = STC_SYNC_DONE;
			}
			break;
		}
		key = get_keyhit();
		if(key==KEY_ESCAPE)
			status = STC_SYNC_NONE;
		else if(key==KEY_BSPACE)
			status = STC_SYNC_REST;
	}
	return status;
}
/*----------------------------------------------------------------------------*/
int stc_packet_pack(stc_dev_t* pdevice, unsigned char* pdata, int dsize)
{
	int loop;
	unsigned short temp;
	pdevice->pcount = 8 + dsize;
	pdevice->packet[0] = STC_PACKET_M0;
	pdevice->packet[1] = STC_PACKET_M1;
	pdevice->packet[2] = STC_PACKET_HOST2MCU;
	temp = pdevice->pcount-2;
	pdevice->packet[3] = (unsigned char)((temp&0xff00)>>8);
	pdevice->packet[4] = (unsigned char)(temp&0xff);
	/* place data */
	for (loop=0;loop<dsize;loop++)
		pdevice->packet[loop+STC_PACKET_DATA_OFFSET] = pdata[loop];
	loop += STC_PACKET_DATA_OFFSET;
	/* get checksum */
	temp = stc_generate_chksum(pdevice);
	pdevice->packet[loop++] = (unsigned char)((temp&0xff00)>>8);
	pdevice->packet[loop++] = (unsigned char)(temp&0xff);
	/* end marker */
	pdevice->packet[loop++] = STC_PACKET_ME;
	/* returns packet size */
	return loop;
}
/*----------------------------------------------------------------------------*/
#define STC_PACKET_WAIT_TIMEOUT -10
#define STC_PACKET_USER_ABORT -20
#define STC_PACKET_VALIDATE_ERROR -30
#define STC_PACKET_HANDSHAKE_ERROR -40
#define STC_PACKET_FLASH_ERROR -50
/*----------------------------------------------------------------------------*/
int stc_packet_send(stc_dev_t* pdevice, serial_port_t* pport)
{
	my1key_t key;
	int loop, test = 0;
	for (loop=0;loop<pdevice->pcount;loop++)
	{
		put_byte_serial(pport,pdevice->packet[loop]);
	}
	while(1)
	{
		if(check_incoming(pport))
		{
			if (stc_wait_packet(pdevice,pport)==SERIAL_TIMEOUT)
				test = STC_PACKET_WAIT_TIMEOUT;
			else if (stc_validate_packet(pdevice)!=STC_PACKET_VALID)
				test = STC_PACKET_VALIDATE_ERROR;
			break;
		}
		key = get_keyhit();
		if(key==KEY_ESCAPE)
		{
			test = STC_PACKET_USER_ABORT;
			break;
		}
	}
	pdevice->error += test;
	return test;
}
/*----------------------------------------------------------------------------*/
int stc_handshake(stc_dev_t* pdevice, serial_port_t* pport)
{
	unsigned char data[] = { pdevice->flag, 0x00, 0x00, 0x36, 0x01,
		(unsigned char)(pdevice->uid0&0xff),
		(unsigned char)(pdevice->uid1&0xff)};
	/* form packet */
	stc_packet_pack(pdevice,data,sizeof(data));
	/* send packet */
	if (!stc_packet_send(pdevice,pport))
	{
		pdevice->flag = pdevice->info.pdata[PAYLOAD_INFO_OFFSET_FLAG];
		if (pdevice->flag!=PAYLOAD_HANDSHAKE_ID||pdevice->info.dsize!=1)
			pdevice->error += STC_PACKET_HANDSHAKE_ERROR;
	}
	return pdevice->error;
}
/*----------------------------------------------------------------------------*/
int stc_bauddance(stc_dev_t* pdevice, serial_port_t* pport)
{
/*
sample_rate = 16 (6T) or 32
brt = 65536 - round((freq)/(baudrate*sample_rate))
brt_csum = (2*(256-brt))&0xff
*/
	int test = -((pdevice->freq*1000000)/(pdevice->baudr*16));
	unsigned char baud = test&0xff;
	unsigned char bsum = (2*(256-(int)baud))&0xff;
	unsigned char dlay = 0x80; /* stc-isp=>0xa0, stc-gal=>0x40 */
	unsigned char iapw = 0x83; /* iap wait register? */
	unsigned char data[] = { pdevice->flag,0xc0,baud,0x3f,bsum,dlay,iapw };
	/* form packet */
	stc_packet_pack(pdevice,data,sizeof(data));
	/* send packet */
	if (!stc_packet_send(pdevice,pport))
	{
		pdevice->flag = pdevice->info.pdata[PAYLOAD_INFO_OFFSET_FLAG];
	}
	return pdevice->error;
}
/*----------------------------------------------------------------------------*/
#define ERASE_COMMAND_SIZE (7+12+128-(14-1))
/*----------------------------------------------------------------------------*/
int stc_erase_mem(stc_dev_t* pdevice, serial_port_t* pport)
{
	int loop;
	unsigned char blks = pdevice->fmemsize*1024/256; /* blocks to erase */
	unsigned char size = pdevice->fmemsize*1024/256; /* number of blocks */
	unsigned char step = 0x80;
	unsigned char data[ERASE_COMMAND_SIZE];
	/* blks & size should be 512-byte aligned (physical size) */
	/* create data */
	data[0] = PAYLOAD_ERASE_MEMORY;
	for (loop=1;loop<19;loop++)
	{
		switch (loop)
		{
			case 3: data[loop] = blks; break;
			case 6: data[loop] = size; break;
			default: data[loop] = 0; break;
		}
	}
	for (loop=19;loop<ERASE_COMMAND_SIZE;loop++,step--)
		data[loop] = step;
	/* form packet */
	stc_packet_pack(pdevice,data,sizeof(data));
	/* send packet */
	if (!stc_packet_send(pdevice,pport))
	{
		pdevice->flag = pdevice->info.pdata[PAYLOAD_INFO_OFFSET_FLAG];
	}
	return pdevice->error;
}
/*----------------------------------------------------------------------------*/
int stc_flash_mem(stc_dev_t* pdevice, serial_port_t* pport)
{
	int loop, next = 0, step, size = STC_FLASH_BLOCK_SIZE_PHYSICAL;
	unsigned char data[STC_PACKET_SIZE], csum;
	/* make sure data size is aligned to flash block size */
	while (size<pdevice->datasize)
		size += STC_FLASH_BLOCK_SIZE_PHYSICAL;
	/* send in 'packets' */
	while (size>0)
	{
		/* create data */
		data[0] = PAYLOAD_FLASH_MEMORY;
		data[1] = 0x00;
		data[2] = 0x00;
		data[3] = ((next&0xff00)>>8)&0xff;
		data[4] = next&0xff;
		data[5] = 0x00; /* sizeh */
		data[6] = STC_FLASH_BLOCK_SIZE; /* sizel */
		/* copy data */
		csum = 0x00;
		for (loop=0,step=7;loop<STC_FLASH_BLOCK_SIZE;loop++,step++,next++)
		{
			if (next<pdevice->datasize)
				data[step] = pdevice->data[next];
			else
				data[step] = 0x00; /* zero-pad */
			csum += data[step];
		}
		/* form packet */
		stc_packet_pack(pdevice,data,step);
		/* send packet */
		if (!stc_packet_send(pdevice,pport))
			pdevice->flag = pdevice->info.pdata[PAYLOAD_INFO_OFFSET_FLAG];
		else break;
		/* do checksum? */
		if ((int)pdevice->info.pdata[1]!=csum)
		{
			pdevice->error += STC_PACKET_FLASH_ERROR;
			break;
		}
		size -= STC_FLASH_BLOCK_SIZE;
	}
	/* finish-up if there is no error! */
	if (!pdevice->error)
	{
		unsigned char data[] = { PAYLOAD_FLASH_FINISH, 0x00, 0x00, 0x36, 0x01,
			(unsigned char)(pdevice->uid0&0xff),
			(unsigned char)(pdevice->uid1&0xff)};
		/* form packet */
		stc_packet_pack(pdevice,data,sizeof(data));
		/* send packet */
		if (!stc_packet_send(pdevice,pport))
			pdevice->flag = pdevice->info.pdata[PAYLOAD_INFO_OFFSET_FLAG];
	}
	return pdevice->error;
}
/*----------------------------------------------------------------------------*/

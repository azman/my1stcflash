/*----------------------------------------------------------------------------*/
#include "my1stc.h"
#include "my1cons.h"
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
#include <sys/time.h>
/*----------------------------------------------------------------------------*/
int stc_check_isp(stc_dev_t* pdevice, serial_port_t* pport, int timeout_ms)
{
	struct timeval inittime, currtime;
	my1key_t key;
	int do_wait = 0;
	int status = STC_SYNC_INIT;
	/* gettimeofday is using micro-second resolution! */
	timeout_ms *= 1000;
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
			gettimeofday(&currtime,0x0);
			if((currtime.tv_sec>inittime.tv_sec)||
				(currtime.tv_usec-inittime.tv_usec)>timeout_ms)
			{
				do_wait = 0;
			}
		}
		if(check_incoming(pport))
		{
			do_wait = 1;
			pdevice->pcount = 0;
			while(do_wait)
			{
				int temp = get_byte_serial_timed(pport,timeout_ms);
				if(temp==SERIAL_TIMEOUT)
					break;
				if(pdevice->pcount<STC_PACKET_SIZE)
				{
					pdevice->packet[pdevice->pcount++] = (byte_t) temp;
				}
				else
				{
					/* if this char is NOT end marker, we're screwed! */
					if(temp!=STC_PACKET_ME)
						status = STC_SYNC_MISS;
					break;
				}
			}
			if(status==STC_SYNC_INIT)
				status = STC_SYNC_DONE;
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
int stc_validate_packet(stc_dev_t* pdevice)
{
	int test = STC_PACKET_VALID, loop;
	/* 16-bit big-endian init marker */
	pdevice->packinfo.imark = pdevice->packet[0]*256 + pdevice->packet[1];
	/* direction */
	pdevice->packinfo.hflag = (int) pdevice->packet[2];
	/* 16-bit big-endian length */
	pdevice->packinfo.dsize = pdevice->packet[3]*256 + pdevice->packet[4];
	/* data pointer */
	pdevice->packinfo.pdata = &pdevice->packet[5];
	pdevice->packinfo.psize = pdevice->packinfo.dsize - 6;
	loop = pdevice->pcount;
	if (loop==STC_PACKET_SIZE)
	{
		/* end marker is implied */
		pdevice->packinfo.emark = STC_PACKET_ME;
	}
	else
	{
		loop--;
		pdevice->packinfo.emark = pdevice->packet[loop];
	}
	/* 16-bit big-endian checksum */
	pdevice->packinfo.cksum = pdevice->packet[loop-2]*256 +
		pdevice->packet[loop-1];
	/* do validation! */
	if(pdevice->packinfo.imark!=STC_PACKET_MX)
		test = STC_PACKET_ERROR_BEGMARK;
	else if(pdevice->packinfo.hflag!=STC_PACKET_MCU2HOST)
		test = STC_PACKET_ERROR_DIRECT;
	else if(pdevice->packinfo.dsize!=pdevice->pcount-2)
		test = STC_PACKET_ERROR_LENGTH;
	else if(pdevice->packinfo.emark!=STC_PACKET_ME)
		test = STC_PACKET_ERROR_ENDMARK;
	else
	{
		/* do checksum */
		unsigned short cksum = 0x0000;
		for(loop=2;loop<pdevice->pcount-3;loop++)
			cksum += pdevice->packet[loop];
		if(pdevice->packinfo.cksum!=cksum)
			test = STC_PACKET_ERROR_CHECKSUM;
	}
	return test;
}
/*----------------------------------------------------------------------------*/

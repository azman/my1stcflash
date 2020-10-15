/*----------------------------------------------------------------------------*/
#include "my1uart.h"
#include "my1keys.h"
#include "my1text.h"
#include "my1list.h"
#include "my1stc.h"
#include "my1stc_db.h"
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define PROGNAME "my1stcflash"
/*----------------------------------------------------------------------------*/
#define COMMAND_NONE 0
#define COMMAND_SCAN 1
/*----------------------------------------------------------------------------*/
#define ERROR_GENERAL -1
#define ERROR_USER_ABORT -2
#define ERROR_PARAM_PORT -3
#define ERROR_PARAM_BAUD -4
#define ERROR_PORT_INIT -5
#define ERROR_PORT_OPEN -6
#define ERROR_PARAM_FILE -7
#define ERROR_MULTI_CMD -8
#define ERROR_NO_COMMAND -9
#define ERROR_NO_HEXFILE -10
/*----------------------------------------------------------------------------*/
#define FILENAME_LEN 80
/*----------------------------------------------------------------------------*/
#define DEFAULT_MCUDB "stcmcudb.txt"
/*----------------------------------------------------------------------------*/
#define DEFAULT_BAUDRATE 9600
/*----------------------------------------------------------------------------*/
void about(void)
{
	printf("\nUse:\n  %s [options] [command]\n",PROGNAME);
	printf("Options are:\n");
	printf("  --help      : show this message. overrides other options.\n");
	printf("  --port []   : specify port number between 1-%d.\n",MAX_COM_PORT);
	printf("  --baud []   : baudrate e.g. 9600(default),38400,115200.\n");
	printf("  --file []   : specify programming file.\n");
	printf("  --tty []    : specify name for device (useful on Linux)\n");
	printf("  --time []   : specify serial time out (microseconds)\n");
	printf("Commands are:\n");
	printf("  scan   : Scan for available serial port.\n");
	printf("\n");
}
/*----------------------------------------------------------------------------*/
void print_portscan(my1uart_t* port)
{
	int test, size = 0;
	printf("--------------------\n");
	printf("COM Port Scan Result\n");
	printf("--------------------\n");
	for (test=1;test<=MAX_COM_PORT;test++)
	{
		if (uart_prep(port,test))
		{
			printf("%s: Ready\n",port->temp);
			size++;
		}
	}
	printf("\nDetected Port(s): %d\n\n",size);
}
/*----------------------------------------------------------------------------*/
char is_whitespace(char achar)
{
	switch(achar)
	{
		case ' ':
		case '\t':
			break;
		default:
			achar = 0x0;
	}
	return achar;
}
/*----------------------------------------------------------------------------*/
#define MCUDB_DELIM " ,\t"
/*----------------------------------------------------------------------------*/
int find_device_list(my1list* list,int uid0,int uid1)
{
	int index = -1, check = 0;
	list->curr = 0x0;
	while(list_iterate(list))
	{
		stcmcu_t *item = (stcmcu_t *) list->curr->item;
		if (item->uid0==uid0&&item->uid1==uid1)
		{
			index = check;
			break;
		}
		check++;
	}
	return index;
}
/*----------------------------------------------------------------------------*/
int get_device_list(my1list* list,char* filename)
{
	char *ibuff, *tbuff, *pbuff;
	stcmcu_t temp;
	my1text_t text;
	int loop;
	/** load predefined devices */
	for(loop=0;loop<STCMCU_DBSIZE;loop++)
	{
		if (find_device_list(list,
			stcmcu_db[loop].uid0,stcmcu_db[loop].uid1)>=0) continue;
		stcmcu_t *item = (stcmcu_t *) malloc(sizeof(stcmcu_t));
		memcpy((void*)item,(void*)&stcmcu_db[loop],sizeof(stcmcu_t));
		list_push_item(list,(void*)item);
	}
	/** try to open dynamic list */
	text_init(&text);
	text_open(&text,filename);
	if (text.pfile)
	{
		while (text_read(&text)>=CHAR_INIT)
		{
			/* skip whitespace to check 'empty' lines */
			ibuff = text.pbuff.buff;
			while(is_whitespace(*ibuff)) ibuff++;
			/* skip if empty lines! also ignore comments? */
			if(ibuff[0]=='\0'||ibuff[0]=='#')
				continue;
			/* create separate token processing buffer */
			tbuff = (char*) malloc(text.pbuff.fill);
			strcpy(tbuff,ibuff);
			/* break down main line components */
			temp.flag = 0;
			do
			{
				/* check if there is a comment */
				pbuff = strchr(tbuff,'#');
				if(pbuff) pbuff[0] = 0x0; /* end at comment! */
				/* look for mcu id1 */
				pbuff = strtok(tbuff,MCUDB_DELIM);
				if (!pbuff) break;
				sscanf(pbuff,"%x",&temp.uid0);
				/* look for mcu id2 */
				pbuff = strtok(0x0,MCUDB_DELIM);
				if (!pbuff) break;
				sscanf(pbuff,"%x",&temp.uid1);
				/* look for mcu name */
				pbuff = strtok(0x0,MCUDB_DELIM);
				if (!pbuff) break;
				sscanf(pbuff,"%s",temp.label);
				/* look for mcu flash size */
				pbuff = strtok(0x0,MCUDB_DELIM);
				if (!pbuff) break;
				sscanf(pbuff,"%d",&temp.fmsz);
				/* look for mcu eeprom size */
				pbuff = strtok(0x0,MCUDB_DELIM);
				if (!pbuff) break;
				sscanf(pbuff,"%d",&temp.emsz);
				temp.flag = 1;
			}
			while(0);
			/* create list item if valid */
			if (temp.flag)
			{
				if (find_device_list(list,temp.uid0,temp.uid1)>=0) continue;
				stcmcu_t *item = (stcmcu_t *) malloc(sizeof(stcmcu_t));
				memcpy((void*)item,(void*)&temp,sizeof(stcmcu_t));
				list_push_item(list,(void*)item);
			}
			/* release text buffer */
			free((void*)tbuff);
		}
		text_done(&text);
	}
	text_free(&text);
	return list->count;
}
/*----------------------------------------------------------------------------*/
int find_devinfo(stc_dev_t* pdevice, my1list* list)
{
	int index = -1;
	list->curr = 0x0;
	while (list_iterate(list))
	{
		index++;
		stcmcu_t *item = (stcmcu_t *) list->curr->item;
		if (item->uid0==pdevice->uid0&&item->uid1==pdevice->uid1)
		{
			strcpy(pdevice->label,item->label);
			pdevice->fmemsize = item->fmsz;
			pdevice->ememsize = item->emsz;
			break;
		}
	}
	return index;
}
/*----------------------------------------------------------------------------*/
#define MAX_HEX_DATA_BYTE 32
#define COUNT_HEXSTR_BYTE (1+2+1+MAX_HEX_DATA_BYTE+1)
#define COUNT_HEXSTR_CHAR (1+COUNT_HEXSTR_BYTE*2+1)
#define COUNT_HEXSTR_BUFF (COUNT_HEXSTR_CHAR+2)
/*----------------------------------------------------------------------------*/
#define HEX_ERROR_GENERAL -1
#define HEX_ERROR_FILE -2
#define HEX_ERROR_LENGTH -3
#define HEX_ERROR_NOCOLON -4
#define HEX_ERROR_NOTDATA -5
#define HEX_ERROR_CHECKSUM -6
#define HEX_ERROR_OVERFLOW -7
#define HEX_ERROR_ACK -8
#define HEX_ERROR_INVALID -9
#define HEX_ERROR_SIZE -10
#define HEX_ERROR_BINSIZE -11
#define HEX_ERROR_MEMBIN -12
/*----------------------------------------------------------------------------*/
typedef struct _linehex
{
	unsigned int count, type;
	unsigned int address;
	unsigned int lastaddr;
	unsigned int data[MAX_HEX_DATA_BYTE];
	char hexstr[COUNT_HEXSTR_BUFF];
}
linehex_t;
/*----------------------------------------------------------------------------*/
int get_hexbyte(const char* hexbyte)
{
	int value;
	char test[3] = { hexbyte[0], hexbyte[1], 0x0 };
	sscanf(test,"%02X",&value);
	return value&0xFF;
}
/*----------------------------------------------------------------------------*/
int check_hex(char* hexstr, linehex_t* linehex)
{
	int length = 0, loop = 0, type;
	int count, address, checksum, test, temp;
	/* find length, filter newline! */
	while(hexstr[length])
	{
		linehex->hexstr[length] = hexstr[length];
		if(hexstr[length]=='\n'||hexstr[length]=='\r')
		{
			hexstr[length] = 0x0;
			linehex->hexstr[length] = 0x0;
			break;
		}
		length++;
		if(length>COUNT_HEXSTR_CHAR)
			return HEX_ERROR_LENGTH;
	}
	/* check valid length */
	if(length<9)
		return HEX_ERROR_LENGTH;
	/* check first char */
	if(hexstr[loop++]!=':')
		return HEX_ERROR_NOCOLON;
	/* get data count */
	count = get_hexbyte(&hexstr[loop]);
	checksum = count&0x0FF;
	if (count>MAX_HEX_DATA_BYTE)
		return HEX_ERROR_SIZE;
	loop += 2;
	/* get address - highbyte */
	test = get_hexbyte(&hexstr[loop]);
	checksum += test&0xFF;
	loop += 2;
	address = (test&0xFF)<<8;
	/* get address - lowbyte */
	test = get_hexbyte(&hexstr[loop]);
	checksum += test&0xFF;
	loop += 2;
	address |= (test&0xFF);
	/* get record type for checksum calc */
	type = get_hexbyte(&hexstr[loop]);
	checksum += type&0xFF;
	loop += 2;
	/* check EOF type? */
	if(type!=0x00&&type!=0x01)
		return HEX_ERROR_NOTDATA;
	/* save data if requested */
	if(linehex)
	{
		linehex->count = count;
		linehex->type = type;
		linehex->address = address;
		linehex->lastaddr = address + count - 1 + type; /* count=0 @ type=1! */
	}
	/* get data */
	for(temp=0;temp<count;temp++)
	{
		if(loop>=(length-2))
			return HEX_ERROR_LENGTH;
		test = get_hexbyte(&hexstr[loop]);
		if(linehex)
			linehex->data[temp] = test&0xFF;
		checksum += test&0xFF;
		loop += 2;
	}
	/* get checksum */
	if(loop!=(length-2))
		return HEX_ERROR_LENGTH;
	test = get_hexbyte(&hexstr[loop]);
	/* calculate and verify checksum */
	checksum = ~checksum + 1;
	if((test&0xFF)!=(checksum&0xFF))
		return HEX_ERROR_CHECKSUM;
	/* returns record type */
	return type;
}
/*----------------------------------------------------------------------------*/
#define MEMORYBIN_MAX 0x10000
/*----------------------------------------------------------------------------*/
typedef struct _memorybin_t
{
	int initaddr, nextaddr, datasize;
	unsigned char* data;
}
memorybin_t;
/*----------------------------------------------------------------------------*/
void init_memorybin(memorybin_t* mem)
{
	mem->initaddr = -1;
	mem->nextaddr = 0x0000;
	mem->datasize = 0;
	mem->data = 0x0;
}
/*----------------------------------------------------------------------------*/
void free_memorybin(memorybin_t* mem)
{
	if (mem->data)
	{
		free((void*)mem->data);
		mem->data = 0x0;
	}
	mem->datasize = 0;
	mem->nextaddr = 0x0000;
	mem->initaddr = -1;
}
/*----------------------------------------------------------------------------*/
int fill_memorybin(memorybin_t* mem, linehex_t* hex)
{
	int size = mem->datasize, loop, addr, prep = 0, posp;
	void *buff;
	/* get non-overlapped space */
	if (mem->initaddr<0)
	{
		prep = hex->address;
		posp = prep + hex->count;
	}
	else
	{
		posp = hex->lastaddr-mem->nextaddr+1;
		if (posp<0) posp = 0;
	}
#ifdef MY1DEBUG
	printf("\n[CHECK] Init:%04x,Next:%04x,Size:%04x,Addr:%04x,Step:%04x",
		mem->initaddr,mem->nextaddr,mem->datasize,hex->address,hex->count);
#endif
	/* need to resize? */
	if (posp>0)
	{
		size += posp;
		if (size>MEMORYBIN_MAX)
			return HEX_ERROR_BINSIZE;
		buff = realloc(mem->data,size);
		if (!buff)
			return HEX_ERROR_MEMBIN;
		/* assume always get same space? */
		mem->data = (unsigned char*) buff;
		mem->datasize = size;
		/* prepend zero-pad? */
		for (loop=0,addr=0;loop<prep;loop++)
			mem->data[addr++] = 0;
		if (mem->initaddr<0) mem->initaddr = 0;
		mem->nextaddr = size;
	}
	/* copy in data */
	addr = hex->address;
	for (loop=0;loop<hex->count;loop++)
		mem->data[addr++] = hex->data[loop];
	return size;
}
/*----------------------------------------------------------------------------*/
int hex2_memorybin(memorybin_t* mem,char* filename)
{
	int test = 0, temp;
	linehex_t linehex;
	my1text_t text;
	text_init(&text);
	text_open(&text,filename);
	if (text.pfile)
	{
		while (text_read(&text)>=CHAR_INIT)
		{
			temp = check_hex(text.pbuff.buff,&linehex);
			if (temp<0)
			{
				test = temp;
				/* point the error and continue? */
				break;
			}
			if (temp) break; /* type 0x01 end of file found! */
			temp = fill_memorybin(mem,&linehex);
			if (temp<0)
			{
				test = temp;
				/* point the error and continue? */
				break;
			}
		}
		text_done(&text);
	}
	else test = HEX_ERROR_FILE;
	text_free(&text);
	return test;
}
/*----------------------------------------------------------------------------*/
void print_device_packet(stc_dev_t* pdevice)
{
	int loop, temp, size = pdevice->pcount;
	printf("---------------\n");
	printf("PACKET:%s\n",
		pdevice->packet[2]==STC_PACKET_HOST2MCU?"host2mcu":
		pdevice->packet[2]==STC_PACKET_MCU2HOST?"mcu2host":"UNKNOWN!");
	printf("---------------\n");
	for (loop=0;loop<size;loop++)
		printf("%02X ",pdevice->packet[loop]);
	if (size>0)
		printf("\n");
	printf("Init: %02X %02X (%02X %02X)\n",
		pdevice->packet[0],pdevice->packet[1],STC_PACKET_M0,STC_PACKET_M1);
	printf("Type: %02X\n",pdevice->packet[2]);
	printf("Flag: %02X\n",pdevice->packet[STC_PACKET_DATA_OFFSET]);
	temp = (int)(pdevice->packet[3]<<8)|pdevice->packet[4];
	printf("Size: %d (%d(@0x%04X)-6)[%d]\n",temp-6,temp,temp,size);
	printf("Mark: %02X (%02X)\n",pdevice->packet[size-1],STC_PACKET_ME);
	printf("CSUM: %02X %02X\n",pdevice->packet[size-3],pdevice->packet[size-2]);
}
/*----------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
	my1uart_t port;
	my1uart_conf_t conf;
	int loop, test, temp, pick=1, baudrate = DEFAULT_BAUDRATE,
		baudhand = DEFAULT_BAUDRATE;
	int time_out = STC_SYNC_TIMEOUT_US;
	int do_command = COMMAND_NONE;
	char *pfile = 0x0, *ptty = 0x0, *plist = 0x0;
	char dlist[] = DEFAULT_MCUDB;
	stc_dev_t device;
	memorybin_t memory;
	my1list mcudb;

	/* print tool info */
	printf("\n%s - STC Flash Tool (version %s)\n",PROGNAME,PROGVERS);
	printf("  => by azman@my1matrix.net\n\n");

	if(argc>1)
	{
		for(loop=1;loop<argc;loop++)
		{
			if(!strcmp(argv[loop],"--help")||!strcmp(argv[loop],"-h"))
			{
				about();
				return 0;
			}
			else if(!strcmp(argv[loop],"--port"))
			{
				if(get_param_int(argc,argv,&loop,&test)<0)
				{
					printf("Cannot get port number!\n");
					return ERROR_PARAM_PORT;
				}
				else if(test<1||test>MAX_COM_PORT)
				{
					printf("Invalid port number! (%d)\n", test);
					return ERROR_PARAM_PORT;
				}
				pick = test;
			}
			else if(!strcmp(argv[loop],"--baud"))
			{
				if(get_param_int(argc,argv,&loop,&test)<0)
				{
					printf("Cannot get transfer baud rate!\n");
					return ERROR_PARAM_BAUD;
				}
				baudrate = test;
			}
			else if(!strcmp(argv[loop],"--hand"))
			{
				if(get_param_int(argc,argv,&loop,&test)<0)
				{
					printf("Cannot get handshake baud rate!\n");
					return ERROR_PARAM_BAUD;
				}
				baudhand = test;
			}
			else if(!strcmp(argv[loop],"--tty"))
			{
				if(!(ptty=get_param_str(argc,argv,&loop)))
				{
					printf("Error getting tty name!\n");
					continue;
				}
			}
			else if(!strcmp(argv[loop],"--time"))
			{
				if(get_param_int(argc,argv,&loop,&test)<0)
				{
					printf("Cannot get timeout value!\n");
					return ERROR_PARAM_BAUD;
				}
				time_out = test;
			}
			else if(!strcmp(argv[loop],"--file"))
			{
				if(!(pfile=get_param_str(argc,argv,&loop)))
				{
					printf("Error getting filename!\n");
					return ERROR_PARAM_FILE;
				}
			}
			else if(!strcmp(argv[loop],"--list"))
			{
				if(!(plist=get_param_str(argc,argv,&loop)))
				{
					printf("Error getting listname!\n");
					return ERROR_PARAM_FILE;
				}
			}
			else if(!strcmp(argv[loop],"scan"))
			{
				if(do_command!=COMMAND_NONE)
				{
					printf("Multiple commands '%s'!(%d)\n",
						argv[loop],do_command);
					return ERROR_MULTI_CMD;
				}
				do_command = COMMAND_SCAN;
			}
			else
			{
				printf("Unknown param '%s'!\n",argv[loop]);
			}
		}
	}

	/** load hex file if requested */
	init_memorybin(&memory);
	if (pfile)
	{
		printf("Loading code HEX file... ");
		temp = hex2_memorybin(&memory, pfile);
#ifdef MY1DEBUG
		printf("\n");
#endif
		if (temp<0)
		{
			printf("fail! (%d)\n",temp);
			return ERROR_GENERAL;
		}
		printf("done! (%d)\n",memory.datasize);
	}

	/** initialize port */
	uart_init(&port);
#ifndef DO_MINGW
	sprintf(port.name,"/dev/ttyUSB"); /* default on linux? */
#endif
	/** check user requested name change */
	if(ptty) sprintf(port.name,ptty);

	/** check if user requested a port scan */
	if(do_command==COMMAND_SCAN)
	{
		print_portscan(&port);
		return 0;
	}

	/** try to prepare port with requested terminal */
	if(!pick) pick = uart_find(&port,0x0);
	if(!uart_prep(&port,pick))
	{
		about();
		print_portscan(&port);
		printf("Cannot prepare port '%s%d'!\n\n",port.name,COM_PORT(pick));
		return ERROR_PORT_INIT;
	}

	/** get the default config */
	uart_get_config(&port,&conf);

	/** apply custom config */
	if(baudhand)
	{
		switch(baudhand)
		{
			case 1200: conf.baud = MY1BAUD1200; break;
			case 2400: conf.baud = MY1BAUD2400; break;
			case 4800: conf.baud = MY1BAUD4800; break;
			case 9600: conf.baud = MY1BAUD9600; break;
			case 19200: conf.baud = MY1BAUD19200; break;
			case 38400: conf.baud = MY1BAUD38400; break;
			case 57600: conf.baud = MY1BAUD57600; break;
			case 115200: conf.baud = MY1BAUD115200; break;
			default:
				printf("Invalid handshake baud (%d)! Using default (%d)!\n",
					baudhand,DEFAULT_BAUDRATE);
				baudhand = DEFAULT_BAUDRATE;
		}
	}
	if(baudrate)
	{
		switch(baudrate)
		{
			case 1200: case 2400: case 4800:
			case 9600: case 19200: case 38400:
			case 57600: case 115200:
				break;
			default:
				printf("Invalid baudrate (%d)! Using default (%d)!\n",
					baudrate,DEFAULT_BAUDRATE);
				baudrate = DEFAULT_BAUDRATE;
		}
	}
	conf.bpar = MY1PARITY_EVEN; /** stc12 uses this! */

	/** set the desired config */
	uart_set_config(&port,&conf);

	/* device interface configuration */
	device.timeout_us = time_out;
	device.error = 0;
	device.baudhand = baudhand;
	device.baudrate = baudrate;
	device.label[0] = 0x0;
	device.data = 0x0;
	if (pfile)
	{
		device.datasize = memory.datasize;
		device.data = memory.data;
	}

	/** load mcu db! */
	list_setup(&mcudb,LIST_TYPE_QUEUE);
	if (!plist) plist = dlist;
	printf("Loading device database... ");
	get_device_list(&mcudb,plist);
	printf("done! (%d)\n",mcudb.count);
#ifdef MY1DEBUG
	printf("Device List:\n");
	mcudb.curr = 0x0;
	while(list_iterate(&mcudb))
	{
		stcmcu_t *item = (stcmcu_t *) mcudb.curr->item;
		printf("{ID0:%d,ID1:%d} => %s\n",item->uid0,item->uid1,item->label);
	}
#endif

	/** try to open port */
	if(!uart_open(&port))
	{
		printf("\nCannot open port '%s%d'!\n\n",port.name,COM_PORT(port.term));
		return ERROR_PORT_OPEN;
	}

	/** start doing things */
	do
	{
		/** clear input buffer */
		uart_purge(&port);
		printf("\nLooking for STC12 device... "); fflush(stdout);
		test = stc_check_isp(&device,&port);
		if (test==STC_SYNC_NONE||test==STC_SYNC_REST)
		{
			printf("user abort. (%d)",test);
			break;
		}
		if (test!=STC_SYNC_DONE)
		{
			printf("error?? (%d)(%d)\n",test,device.error);
			if (device.pcount>0)
				print_device_packet(&device);
			device.error = 0; /* start new! */
			continue;
		}
		printf("found! ");
		printf("\nBaudrate(H): %d",device.baudhand);
		printf("\nBaudrate(D): %d",device.baudrate);
		if (find_devinfo(&device,&mcudb)<0)
		{
			printf("\nUnknown device (%01x/%01x)!",
				device.uid0,device.uid1);
		}
		else
		{
			printf("\nMCU Dev#: %s",device.label);
		}
		printf("\nMCU Freq: %.4f MHz",device.freq);
		printf("\nFirmware: %d.%d%c (Payload: 0x%02x)",
				device.fw11,device.fw12,(char)device.fw20,device.flag);
		printf("\nFlash  Size: %2d kB",device.fmemsize);
		printf("\nEEPROM Size: %2d kB",device.ememsize);
		printf("\nInit handshake... "); fflush(stdout);
		test = stc_handshake(&device,&port);
		if (!test) printf("success! {%02x}",device.flag);
		else
		{
			printf("error! (%d)\n",test);
			print_device_packet(&device);
			exit(1);
		}
		/* test the baudrate */
		printf("\nInit baud test... "); fflush(stdout);
		test = stc_bauddance(&device,&port);
		if (!test) printf("success! {%02x}(%d%%)",device.flag,device.baud_err);
		else
		{
			printf("error! (%d)\n",test);
			print_device_packet(&device);
			exit(1);
		}
		/* confirm the baudrate? */
		device.flag = PAYLOAD_BAUD_CONFIRM;
		printf("\nDone baud test... "); fflush(stdout);
		test = stc_bauddance(&device,&port);
		if (!test) printf("success! {%02x}(%d%%)",device.flag,device.baud_err);
		else
		{
			printf("error! (%d)\n",test);
			print_device_packet(&device);
			exit(1);
		}
		/* continue only if we are flashing */
		if (pfile)
		{
			printf("\nErase memory... "); fflush(stdout);
			test = stc_erase_mem(&device,&port);
			if (!test) printf("success! {%02x}",device.flag);
			else
			{
				printf("error! (%d)\n",test);
				print_device_packet(&device);
				exit(1);
			}
			printf("\nFlash memory... "); fflush(stdout);
			test = stc_flash_mem(&device,&port);
			if (!test) printf("success! {%02x}",device.flag);
			else
			{
				printf("error! (%d)\n",test);
				print_device_packet(&device);
				exit(1);
			}
			printf("\nSend device options... "); fflush(stdout);
			test = stc_send_opts(&device,&port);
			if (!test) printf("success! {%02x}",device.flag);
			else
			{
				printf("error! (%d)\n",test);
				print_device_packet(&device);
				exit(1);
			}
			printf("\nReset device... "); fflush(stdout);
			test = stc_reset_dev(&device,&port);
			if (!test) printf("success! {%02x}",device.flag);
			else
			{
				printf("error! (%d)\n",test);
				print_device_packet(&device);
				exit(1);
			}
		}
		break;
	}
	while (1); /* for continue to work! */
	printf("\n\n");

	/** cleanup */
	list_clean(&mcudb,&list_free_item);
	free_memorybin(&memory);

	/** we are done */
	uart_done(&port);

	return 0;
}
/*----------------------------------------------------------------------------*/

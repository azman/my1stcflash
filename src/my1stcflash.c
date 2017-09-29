/*----------------------------------------------------------------------------*/
#include "my1comlib.h"
#include "my1cons.h"
#include "my1stc.h"
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
/*----------------------------------------------------------------------------*/
#define PROGNAME "my1stcflash"
/*----------------------------------------------------------------------------*/
#define NXP_FIND_WAIT 10000
/*----------------------------------------------------------------------------*/
#define COMMAND_NONE 0
#define COMMAND_SCAN 1
#define COMMAND_DEVICE 2
#define COMMAND_BCHECK 3
#define COMMAND_ERASE 4
#define COMMAND_WRITE 5
#define COMMAND_VERIFY 6
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
#define ERROR_NO_DEVICE -11
/*----------------------------------------------------------------------------*/
#define FILENAME_LEN 80
/*----------------------------------------------------------------------------*/
void about(void)
{
	printf("Use:\n  %s [options] [command]\n",PROGNAME);
	printf("Options are:\n");
	printf("  --help      : show this message. overrides other options.\n");
	printf("  --port []   : specify port number between 1-%d.\n",MAX_COM_PORT);
	printf("  --baud []   : baudrate e.g. 9600(default),38400,115200.\n");
	printf("  --file []   : specify programming file.\n");
	printf("  --tty []    : specify name for device (useful on Linux)\n");
	printf("  --device [] : specify NXP device (not used for now).\n");
	printf("  --no-device : just as a general terminal.\n");
	printf("  --force     : force erase even when device is blank.\n");
	printf("Commands are:\n");
	printf("  scan   : Scan for available serial port.\n");
	printf("  info   : Display device information.\n");
	printf("  bcheck : Blank check device.\n");
	printf("  erase  : Erase device.\n");
	printf("  write  : Write HEX file to device.\n");
	printf("  verify : Verify HEX file on device.\n");
	printf("\n");
}
/*----------------------------------------------------------------------------*/
void print_portscan(ASerialPort_t* aPort)
{
	int test, cCount = 0;
	printf("--------------------\n");
	printf("COM Port Scan Result\n");
	printf("--------------------\n");
	for(test=1;test<=MAX_COM_PORT;test++)
	{
		if(check_serial(aPort,test))
		{
			printf("%s%d: ",aPort->mPortName,COM_PORT(test));
			cCount++;
			printf("Ready.\n");
		}
	}
	printf("\nDetected Port(s): %d\n\n",cCount);
}
/*----------------------------------------------------------------------------*/
int get_actual_baudrate(int encoded_rate)
{
	int baudrate = 0;
	switch(encoded_rate)
	{
		case MY1BAUD1200: baudrate = 1200; break;
		case MY1BAUD2400: baudrate = 2400; break;
		case MY1BAUD4800: baudrate = 4800; break;
		case MY1BAUD9600: baudrate = 9600; break;
		case MY1BAUD19200: baudrate = 19200; break;
		case MY1BAUD38400: baudrate = 38400; break;
		case MY1BAUD57600: baudrate = 57600; break;
		case MY1BAUD115200: baudrate = 115200; break;
	}
	return baudrate;
}
/*----------------------------------------------------------------------------*/
void print_currtime(FILE* pfile)
{
	double test;
	struct timeval inittime;
	gettimeofday(&inittime,0x0);
	test = (double)inittime.tv_sec;
	test += (double)inittime.tv_usec/1000000;
	fprintf(pfile,"[%20.6lf]",test);
}
/*----------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
	ASerialPort_t cPort;
	ASerialConf_t cConf;
	int loop, test, port=1, baudrate = 0;
	int do_command = COMMAND_NONE;
	char *pfile = 0x0, *ptty = 0x0;
	stc_dev_t device;

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
				port = test;
			}
			else if(!strcmp(argv[loop],"--baud"))
			{
				if(get_param_int(argc,argv,&loop,&test)<0)
				{
					printf("Cannot get baud rate!\n\n");
					return ERROR_PARAM_BAUD;
				}
				baudrate = test;
			}
			else if(!strcmp(argv[loop],"--tty"))
			{
				if(!(ptty=get_param_str(argc,argv,&loop)))
				{
					printf("Error getting tty name!\n");
					continue;
				}
			}
			else if(!strcmp(argv[loop],"--file"))
			{
				if(!(pfile=get_param_str(argc,argv,&loop)))
				{
					printf("Error getting filename!\n");
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

	/** initialize port */
	initialize_serial(&cPort);
#ifndef DO_MINGW
	sprintf(cPort.mPortName,"/dev/ttyUSB"); /* default on linux? */
#endif
	/** check user requested name change */
	if(ptty) sprintf(cPort.mPortName,ptty);

	/** check if user requested a port scan */
	if(do_command==COMMAND_SCAN)
	{
		print_portscan(&cPort);
		return 0;
	}

	/** try to prepare port with requested terminal */
	if(!port) port = find_serial(&cPort,0x0);
	if(!set_serial(&cPort,port))
	{
		about();
		print_portscan(&cPort);
		printf("Cannot prepare port '%s%d'!\n\n",cPort.mPortName,
			COM_PORT(port));
		return ERROR_PORT_INIT;
	}

	/** get the default config */
	get_serialconfig(&cPort,&cConf);

	/** apply custom config */
	if(baudrate)
	{
		switch(baudrate)
		{
			case 1200: cConf.mBaudRate = MY1BAUD1200; break;
			case 2400: cConf.mBaudRate = MY1BAUD2400; break;
			case 4800: cConf.mBaudRate = MY1BAUD4800; break;
			default: printf("Invalid baudrate (%d)! Using default!\n", test);
			case 9600: cConf.mBaudRate = MY1BAUD9600; break;
			case 19200: cConf.mBaudRate = MY1BAUD19200; break;
			case 38400: cConf.mBaudRate = MY1BAUD38400; break;
			case 57600: cConf.mBaudRate = MY1BAUD57600; break;
			case 115200: cConf.mBaudRate = MY1BAUD115200; break;
		}
	}
	cConf.mParity = MY1PARITY_EVEN; /** stc12 uses this! */

	/* device interface configuration */
	device.timeout_us = STC_SYNC_TIMEOUT_US;

	/** set the desired config */
	set_serialconfig(&cPort,&cConf);

	/** try to open port */
	if(!open_serial(&cPort))
	{
		printf("\nCannot open port '%s%d'!\n\n",cPort.mPortName,
			COM_PORT(cPort.mPortIndex));
		return ERROR_PORT_OPEN;
	}

	/** clear input buffer */
	purge_serial(&cPort);

	/** start doing things */
	printf("\nLooking for STC12 device... ");
	test = stc_check_isp(&device,&cPort);

	if (test==STC_SYNC_DONE&&stc_check_info(&device)==STC_PACKET_VALID)
	{
		printf("found! ");
		print_currtime(stdout);
		printf("\nFirmware: %d.%d%c (Payload: 0x%02x)",
				device.fw11,device.fw12,(char)device.fw20,device.flag);
		printf("\nMCU Freq: %.4f MHz",device.freq);
		if ((device.uid0==STC_DEVICE_12C5A60S2_MID1)&&
			(device.uid1==STC_DEVICE_12C5A60S2_MID2))
		{
			printf("\nMCU Dev#: STC12C5A60S2");
		}
		else
		{
			printf("\nUnknown device (%01x/%01x)!",
				device.uid0,device.uid1);
		}
		printf("\n[CHECK] ");
		print_currtime(stdout);
		//device.timeout_us = 500000;
		printf("\nInit handshake... ");
		test = stc_handshake(&device,&cPort);
		printf("done (%d){%d}",test,device.pcount);
		printf("\nHandshake Packet: ");
		for(loop=0;loop<device.pcount;loop++)
			printf("[%02X]",device.packet[loop]);
	}
	else printf("error? (%d)",test);
	putchar('\n');

	/** we are done */
	close_serial(&cPort);

	return 0;
}
/*----------------------------------------------------------------------------*/

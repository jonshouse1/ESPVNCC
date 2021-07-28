/* 
	YAFDP Server
	Active parse and response for YAFDP protocol
	Common code, compiles for linux/esp8266/esp32

	Version 0.10
	Last Modified 4 Jul 2021
 */

#ifndef ESP8266		
	#include <stdio.h> 
	// If you run out of memory on the ESP8266 you probably forgot to add -DESP8266 to CFLAGS in Makefile
#else										// 8266 specific includes
	#include <ets_sys.h>
	#include <osapi.h>
	#include <os_type.h>
	#include <mem.h>
	#include <ets_sys.h>
	#include <osapi.h>
	#include <user_interface.h>
	#include <espconn.h>
	#include "uart.h"
#endif

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "yet_another_functional_discovery_protocol.h"

#ifndef TRUE
	#define TRUE 1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

extern int yafdp_debug;
char yafdp_manufacturer[17];
char yafdp_modelname[33];
char yafdp_location[33];
char yafdp_device_description[33];


// Prototype
#ifndef ESP8266
	int os_printf(char *fmt, ...);
#endif


#ifdef ESP8266
void ICACHE_FLASH_ATTR yafdp_server_init(char *manufacturer, char *modelname, char *device_description, char *location)
#else
void yafdp_server_init(char *manufacturer, char *modelname, char *device_description, char *location)
#endif
{
	// ensure unused bytes are zero
	bzero(&yafdp_manufacturer,sizeof(yafdp_manufacturer));
	bzero(&yafdp_modelname,sizeof(yafdp_modelname));
	bzero(&yafdp_location,sizeof(yafdp_location));
	bzero(&yafdp_device_description,sizeof(yafdp_device_description));
	// use strncpy to clip string if longer than the structure field length
	strncpy((char*)&yafdp_manufacturer, manufacturer, sizeof(yafdp_manufacturer)-1);
	strncpy((char*)&yafdp_modelname, modelname, sizeof(yafdp_modelname)-1);
	strncpy((char*)&yafdp_location, location, sizeof(yafdp_location)-1);
	strncpy((char*)&yafdp_device_description, device_description, sizeof(yafdp_device_description)-1);

	//if (yafdp_debug==TRUE)
	//{
		os_printf("Manfacturer = %s\n",yafdp_manufacturer);
		os_printf("Model name = %s\n",yafdp_modelname);
		os_printf("Device Description = %s\n",yafdp_device_description);
		os_printf("Location = %s\n",yafdp_location);
	//}
}



// This can also be used as an announce if called with ipaddr 255.255.255.255 and request_handle 0
#ifdef ESP8266
void ICACHE_FLASH_ATTR send_yafdp_discovery_reply_device(char *ipaddr,  int vers, int req_handle, int port)
#else
void send_yafdp_discovery_reply_device(char *ipaddr,  int vers, int req_handle, int port)
#endif
{
	char txbuf[sizeof(struct yafdp_reply_device)];
	struct yafdp_reply_device *yafdpreplydev=(struct yafdp_reply_device*)&txbuf;
	int udp_port=YAFDP_DISCOVERY_PORT;								// default for version 0.1 of protocol
	int bcast=FALSE;

	if (strcmp(ipaddr,"255.255.255.255")==0)
		bcast=TRUE;

	bzero(&txbuf,sizeof(txbuf));
	if (port>1024)
		udp_port=port;

	// Build reply packet
	strcpy(yafdpreplydev->magic,YAFDP_MAGIC);
	yafdpreplydev->ptype = YAFDP_TYPE_DISCOVERY_REPLY_DEVICE;
	yafdpreplydev->pver[0] = 0;
	yafdpreplydev->pver[1] = vers;
	yafdpreplydev->request_handle = req_handle;
	yafdpreplydev->number_of_services=0;

	// use strncpy to clip string if longer than the structure field length
	strcpy(yafdpreplydev->device_manufacturer, yafdp_manufacturer);
	strcpy(yafdpreplydev->device_modelname, yafdp_modelname);
	strcpy(yafdpreplydev->device_description, yafdp_device_description);
	strcpy(yafdpreplydev->device_location, yafdp_location);
	udp_generic_send((char*)&txbuf, sizeof(struct yafdp_reply_device), (char*)ipaddr, udp_port, bcast);
	if (yafdp_debug==TRUE)
	{
		os_printf(">%s\tYAFDP_TYPE_DISCOVERY_REPLY_DEVICE, VER:%d, REQH:%d PORT:%d BCASET:%d\n",ipaddr,vers,req_handle,udp_port,bcast);
	}
}



#ifdef ESP8266
void ICACHE_FLASH_ATTR yafdp_parse_and_reply(char *rbuffer, int bsize, char *ipaddr)
#else
void yafdp_parse_and_reply(char *rbuffer, int bsize, char *ipaddr)
#endif
{
	//int r=0;
	static int mute_handle=0;
        struct yafdp_request_devices *yafdprd=(struct yafdp_request_devices*)rbuffer;

        // Overlay these over the buffer for the transmitted reply
	//char txbuf[2048];
        //struct yafdp_reply_device *yafdpreplydev=(struct yafdp_reply_device*)&txbuf;

	if (strcmp(yafdprd->magic,YAFDP_MAGIC)==0)
	{
		switch (yafdprd->ptype) 
		{
			case YAFDP_TYPE_DISCOVERY_REQUEST_DEVICES :
				if (yafdp_debug==TRUE)
				{
					os_printf("<%s\tYAFDP_TYPE_DISCOVERY_REQUEST_DEVICES, REQH:%d\n",ipaddr,yafdprd->request_handle);
				}
				if (yafdprd->request_handle != mute_handle)
				{
					send_yafdp_discovery_reply_device(ipaddr,  yafdprd->pver[1], yafdprd->request_handle, yafdprd->udp_reply_port);
				}
			break;



			// for a given "request_handle" ignore any subsequent "YAFDP_TYPE_DISCOVERY_REQUEST_DEVICES" messages
			// same structure as "request devices, but in this context "request_handle" is the handle we wish to ignore
			case YAFDP_TYPE_DISCOVERY_REQUEST_MUTE :
				mute_handle = yafdprd->request_handle;
				if (yafdp_debug==TRUE)
				{
					os_printf("<%s\tYAFDP_TYPE_DISCOVERY_REQUEST_MUTE, REQH:%d\n",ipaddr,mute_handle);
				}
			break;



			case YAFDP_TYPE_DISCOVERY_REPLY_DEVICE :
				if (yafdp_debug==TRUE)
				{
					os_printf("<%s\tYAFDP_TYPE_DISCOVERY_REPLY_DEVICE\n",ipaddr);
				}
			break;
		}
	}
}



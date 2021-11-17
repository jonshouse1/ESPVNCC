/* 
 * lcdtouchvnc.c
 * ESP32 VNC Client for LCD Touch screens.
 *
 *
 * Copyright (c) 2021 Jonathan Andrews. All rights reserved.
 * This file is part of ESPVNCC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

//#define USE_WIFI			// Uncomment this line for Wifi rather than Ethernet
//#define WIFI_SSID		"YOUR SSID HERE"
//#define WIFI_PASS		"YOUR PASSWORD HERE"

#define VNC_SERVER_IPADDR 	"192.168.1.111"
#define VNC_SERVER_SCREEN_NUM	1


#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"

#include "global.h"
#include "lcd_ts_init.h"
#include "lcd_textbuf.h"
#include "jag.h"
#include "lcd_vncc.h"


scr_driver_t				lcd_drv; 
touch_panel_driver_t 			touch_drv;
const char 				*TAG = "lcdtouchvnc";
char					client_ip[16];
char					server_ip[16];
int					online = FALSE;					// TRUE = connected with an IP address?



void doconnect()
{
	vncc_connect(VNC_SERVER_IPADDR, VNC_SERVER_SCREEN_NUM);
}



void app_main(void)
{
	bzero(&lcd_drv, sizeof(scr_driver_t));
	lcd_init(240, 320);									// initialise the SPI LCD driver

	// Change LCD rotation
	// possible rotations   portrait:   SCR_DIR_LRTB,  SCR_DIR_LRBT,  SCR_DIR_RLTB,  SCR_DIR_RLBT
	//                      landscape:  SCR_DIR_TBLR,  SCR_DIR_BTLR,  SCR_DIR_TBRL,  SCR_DIR_BTRL
	//lcd_ts_rotate(SCR_DIR_TBLR);								// comment out for default potrait LRBT

	jag_init((scr_driver_t*)&lcd_drv);							// initialise my graphics library
	lcd_textbuf_init(&Font12, -1, -1, -1, -1);						// initialise the text terminal
	lcd_textbuf_setcolors(COLOR_WHITE, COLOR_BLUE);
	lcd_textbuf_enable(TRUE, TRUE);								// text terminal active and clear display
	lcd_textbuf_printstring("POE LCD VNC V0.13 ...\n\n");

#ifdef USE_WIFI
	awifi_init(WIFI_SSID, WIFI_PASS);
#else
	aethernet_init();
#endif
	xTaskCreate(yafdp_server_task, "yafdp_server", 16384, NULL, 0, NULL);			// 0 = lowest priority
}




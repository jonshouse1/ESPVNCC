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


#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "global.h"
#include "lcd_ts_init.h"
#include "lcd_textbuf.h"
#include "jag.h"
#include "lcd_vncc.h"


extern const char *TAG;

extern  char	client_ip[16];
extern  int	online;

static EventGroupHandle_t s_wifi_event_group;


#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
	char st[256];
    
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		esp_wifi_connect();
	} 
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
	{
		online=FALSE;
		client_ip[0]=0;
		if (s_retry_num < 40) 
		{
			esp_wifi_connect();
			s_retry_num++;
			sprintf(st,"retry connect to the AP");
			ESP_LOGI(TAG, "%s", st);
			lcd_textbuf_printstring(st);
			lcd_textbuf_printstring("\n");
		} 
		else 
		{
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		sprintf(st,"connect to the AP failed");
		ESP_LOGI(TAG,"%s",st);
		lcd_textbuf_printstring(st);
		lcd_textbuf_printstring("\n");
	} 
	else 
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
	{
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		sprintf(st, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		ESP_LOGI(TAG,"%s",st);
		lcd_textbuf_printstring(st);
		lcd_textbuf_printstring("\n");
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
		online=TRUE;
		sprintf(client_ip, IPSTR, IP2STR(&event->ip_info.ip));
		doconnect();
	}
}


void wifi_init_sta(char* issid, char* ipass)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    
	wifi_config_t wifi_config = 
	{
        	.sta = 
		{
			//.ssid = EXAMPLE_ESP_WIFI_SSID,
            		//.password = EXAMPLE_ESP_WIFI_PASS,
      			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
            		.pmf_cfg = 
			{
                		.capable = true,
                		.required = false
            		},
        	},
    	};
	strncpy((char*)wifi_config.sta.ssid, (char*)issid, sizeof(wifi_config.sta.ssid));
	strncpy((char*)wifi_config.sta.password, (char*)ipass, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 issid, ipass);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 issid, ipass);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}



void awifi_init(char *ssid, char* pass)
{
	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	wifi_init_sta(ssid, pass);
}



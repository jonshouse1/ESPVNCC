/* 
	yafdp_server_task_esp32.c
	YAFDP Server for ESP32 FreeRTOS Espressif SDK
 */

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "global.h"
#include "yet_another_functional_discovery_protocol.h"

#define TRUE	1
#define FALSE	0


extern const char *TAG;
int  yafdp_debug=TRUE;
int  number_of_services=0;




void yafdp_server_task(void *pvParameters)
{
	char rx_buffer[2048];
	char addr_str[128];
	int addr_family;
	int ip_protocol;
        struct sockaddr_in destAddr;
	int len=0;


	while (1) 
	{
        	destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        	destAddr.sin_family = AF_INET;
        	destAddr.sin_port = htons(YAFDP_DISCOVERY_PORT);
        	addr_family = AF_INET;
        	ip_protocol = IPPROTO_IP;
        	inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);
            	struct sockaddr_in6 sourceAddr; 						// Large enough for both IPv4 or IPv6

        	int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        	if (sock < 0) 
		{
            		ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            		break;
        	}
        	ESP_LOGI(TAG, "Socket created");

        	int err = bind(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        	if (err < 0) 
		{
            		ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        	}
        	ESP_LOGI(TAG, "Socket binded");


		yafdp_server_init("ESP32-test", "ESP32", "probably ESP32 dev board", "ESP32 location");
            	socklen_t socklen = sizeof(sourceAddr);
        	while (1) 
		{
            		//ESP_LOGI(TAG, "Waiting for data");
            		len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&sourceAddr, &socklen);
			//printf("len=%d\n",len);
            		if (len < 0)								// Error occured during receiving 
			{
                		ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                		break;
            		}
            		else									// Data received 
			{
                		// Get the sender's ip address as string
                		if (sourceAddr.sin6_family == PF_INET) 
				{
                    			inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                		} 
				else if (sourceAddr.sin6_family == PF_INET6) 
				{
                    			inet6_ntoa_r(sourceAddr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                		}

                		//ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
				//DumpHex(rx_buffer,len);
				yafdp_parse_and_reply((char*)&rx_buffer, sizeof(rx_buffer), (char*)&addr_str);
				//taskYIELD();
            		}
			vTaskDelay(50);          // milliseconds
        	}

        	if (sock != -1) 
		{
            		ESP_LOGE(TAG, "Shutting down socket and restarting...");
            		shutdown(sock, 0);
            		close(sock);
        	}
    }
    vTaskDelete(NULL);
}






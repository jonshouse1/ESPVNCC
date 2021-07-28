/*
	udp_generic_send.c
	ESP32 version
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


int udp_generic_send(char *d, int len, char *destination_ip, int destination_port, int broadcast)
{
        static int sockfd=-1;
        int broadcastx=1;
        struct sockaddr_in sendaddr;
        int numbytes;

        if (sockfd<0)
        {
                if((sockfd = socket(PF_INET,SOCK_DGRAM,0)) == -1)
                {
                        perror("sockfd");
                        exit(1);
                }
        }

        if (broadcast==1)		// 1=TRUE
	{
                if((setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&broadcastx,sizeof broadcastx)) == -1)
                {
                        perror("setsockopt - SO_SOCKET ");
                        exit(1);
                }
        }

	//printf("udp_generic_send: sending %d bytes to %s port %d\n",len,destination_ip,destination_port);
        sendaddr.sin_family = AF_INET;
        sendaddr.sin_port = htons(destination_port);
        sendaddr.sin_addr.s_addr = inet_addr(destination_ip);
        memset(sendaddr.sin_zero,'\0',sizeof sendaddr.sin_zero);
        numbytes = sendto(sockfd, d, len , 0, (struct sockaddr *)&sendaddr, sizeof sendaddr);
        //close(sockfd);
        return(numbytes);
}



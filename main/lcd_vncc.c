/* 
 * lcd_vncc.c
 * VNC client (vncc)
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

#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"

#include "screen_driver.h"
#include "touch_panel.h"

#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include <time.h>

#include "global.h"
#include "lcd_textbuf.h"
#include "lcd_ts_init.h"
#include "lcd_vncc.h"
#include "jag.h"
#include "endian.h"

extern touch_panel_driver_t	touch_drv;

static char		vncc_rxbuf[2048];
static char		vncc_txbuf[512];
char 			vncc_host_ip[22];

extern const char *TAG;
extern int 		online;
extern int  		link_up;
extern int 		connection_state;
extern int 		backlight;

static int		vncc_sock		= -1;
static int		vncc_state 		= VNCC_NOT_CONNECTED;
static int		vncc_taskcreated	= FALSE;
static int		vncc_busy 		= FALSE;						// communicate between tasks
static int		vncc_update_rate_hz 	= 50;
static int		did_draw		= FALSE;						// true the moment we draw some pixels
int			vncc_screennum		= 1;
int			vncc_port		= 0;

struct vnc_ServerInit	vncc_si;									// Keep a copy for reference
char			si_name[32];


void vncc_shutdown()
{
	static int inprogress=FALSE;									// Multiple tasks can call us

	if (inprogress==TRUE)										// function already in progress?
		return;											// then skip it
	inprogress = TRUE;
	if (vncc_sock >0)										// still connected ?
	{
		shutdown(vncc_sock, 0);									// tell host we are leaving
		lcd_textbuf_enable(TRUE, did_draw);							// Back to text mode optionally CLS
		did_draw = FALSE;
	}
	close(vncc_sock);
	vncc_sock = -1;
	vncc_state = VNCC_NOT_CONNECTED;
	inprogress = FALSE;
}



// Read n bytes from socket(fd) into buf b with persistant tries for when sockets is giving short reads
int readbytes(int fd, char*buf , int n)
{
	int bytesread=0;
	int bytestoread=0;
	int cs=512;											// chunk size
	int len=0;

	if (fd<0)
		return(-1);
	bytestoread=n;
	if (n<cs)											// reading less than chunk size?
		cs=n;											// try and read it all in one go
	do
	{
		len = recv(fd, buf+bytesread, cs, 0);
		if (len<0)										// socket read error ?
		{
			vncc_shutdown();
			return(-1);
		}
		if (len>0)
		{
			if (len<cs)									// read less than expected
				vTaskDelay(10 / portTICK_PERIOD_MS);					// yeild to let buffer fill
			bytesread = bytesread + len;
			bytestoread = bytestoread - len;
		}
		if (bytestoread < cs)									// last bit ?
			cs=bytestoread;									// then do one read that size
	} while (bytestoread > 0);
	return(n);
}




void vncc_doconnect()
{
        struct sockaddr_in dest_addr;
	int addr_family = 0;
	int ip_protocol = 0;
	char st[64];

	vncc_port = 5900+vncc_screennum;
        dest_addr.sin_addr.s_addr = inet_addr(vncc_host_ip);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(vncc_port);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

	vncc_sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (vncc_sock < 0) 
	{
		vncc_sock=-1;
		ESP_LOGE(TAG, "Failed to create socket: err %d", errno);
		return;
	}
	sprintf(st,"Trying %s:%d", vncc_host_ip, vncc_port);  
        ESP_LOGI(TAG, "%s",st);
	lcd_textbuf_printstring(st);
	lcd_textbuf_printstring("\n");
	int err = connect(vncc_sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
        if (err != 0) 
	{
		sprintf(st,"Can't connect, err %d", errno);
		lcd_textbuf_printstring(st);
		lcd_textbuf_printstring("\n");
		ESP_LOGE(TAG, "%s", st);
		close(vncc_sock);
		vncc_sock=-1;
		return;
        }
}



void dumphex(char *hex, int l)
{
	int i=0;

	for (i=0;i<l;i++)
		printf("%02X ",hex[i]);
	printf("\n");
}



void print_si(struct vnc_ServerInit* si)
{
	printf("width=%u\n",si->fbwidth);
	printf("height=%u\n",(unsigned int)si->fbheight);
	printf("bpp=%u\n",(unsigned int)si->pf_bpp);
	printf("depth=%u\n",(unsigned int)si->pf_depth);
	printf("namelen=%u\n",(unsigned int)si->namelen);
	printf("name=%s\n",si_name);
}


void process_server_init(struct vnc_ServerInit* si)
{
	int len=0;

	si->fbwidth	= bswap16(si->fbwidth);
	si->fbheight	= bswap16(si->fbheight);
	si->namelen	= bswap32(si->namelen);
	si->pf_maxred	= bswap16(si->pf_maxred);
	si->pf_maxgreen	= bswap16(si->pf_maxgreen);
	si->pf_maxblue	= bswap16(si->pf_maxblue);
	memcpy((struct vnc_ServerInit*)&vncc_si, si, sizeof(struct vnc_ServerInit));		// Use the copy from now on

	bzero(&vncc_rxbuf,sizeof(vncc_rxbuf));
	len = readbytes(vncc_sock, (char*)&vncc_rxbuf, vncc_si.namelen);			// Read remainder of server_init
	if (len==vncc_si.namelen)
	{
		printf("read name, got %d bytes\n",len);
		strncpy((char*)&si_name, (char*)&vncc_rxbuf, sizeof(si_name));
		si_name[sizeof(si_name)-1]=0;
		print_si(&vncc_si);
	}
	else	printf("ERR\n");
}



// Ask server for a message about part of its frame buffer
static void vncc_send_framebuffer_update_request(int x, int y, int w, int h, uint8_t increm)
{
	struct vnc_FramebufferUpdateRequest	fbur;
	int len=0;

	//printf("send_framebuffer_update_request() x=%d\ty=%d\tw=%d\th=%d\tincrem=%d\n",x,y,w,h,increm);
	fbur.msg_type		= VNC_CMT_FRMAEBUFFERUPDATEREQUEST;
	fbur.increm		= increm;							// 1=TRUE 0=FALSE
	fbur.xpos		= bswap16(x);
	fbur.ypos		= bswap16(y);
	fbur.width		= bswap16(w);
	fbur.height		= bswap16(h);
	len = send(vncc_sock, (char*)&fbur, sizeof(struct vnc_FramebufferUpdateRequest), 0);	// Ask for some pixels
	if (len!=sizeof(struct vnc_FramebufferUpdateRequest))
	{
		ESP_LOGE(TAG,"Got %d on send()  vncc_sock=%d",len,vncc_sock);
		vncc_busy = TRUE;
		vncc_shutdown();
	}
}


void vncc_send_pointer_event(uint16_t x, uint16_t y, uint8_t msk)
{
	struct	vnc_PointerEvent	pev;
	int	len=0;

	pev.msg_type	= 5;
	pev.button_mask	= msk;
	pev.xpos	= bswap16(x);
	pev.ypos	= bswap16(y);
	len = send(vncc_sock, (char*)&pev, sizeof(struct vnc_PointerEvent), 0);
	if (len!=sizeof(struct vnc_PointerEvent))
	{
		ESP_LOGE(TAG,"vncc_send_pointer_event() - expected %d got %d",sizeof(struct vnc_PointerEvent), len);
		vncc_shutdown();
	}
}



void vncc_send_setencodings()
{
	struct	vnc_SetEncodings	se;
	struct  vnc_send_encoding_type	et;
	int 	len=0;

	se.msg_type = VNC_CMT_SETENCODINGS; 
	se.padding = 0;
	se.number_of_encodings = bswap16(1);							// List of 1 at the moment	
	len = send(vncc_sock, (char*)&se, sizeof(struct vnc_SetEncodings), 0);
	if (len!=sizeof(struct vnc_SetEncodings))
	{
		ESP_LOGE(TAG,"vncc_send_setencodings() se - expected %d got %d",sizeof(struct vnc_SetEncodings), len); 
		vncc_shutdown();
		return;
	}

	et.encoding_type = bswap32(VNC_ET_RAW);							// Send the list of 1
	len = send(vncc_sock, (char*)&et, sizeof(struct vnc_send_encoding_type), 0);
	if (len!=sizeof(struct vnc_SetEncodings))
	{
		ESP_LOGE(TAG,"vncc_send_setencodings() et - expected %d got %d",sizeof(struct vnc_SetEncodings), len); 
		vncc_shutdown();
		return;
	}
}



// Try and stay in sync with protocol by throwing away data when we seem out of sync
// This can be removed when everything is working as expected
static void vncc_drain(char *s)
{
	int len=0;

	vncc_busy = TRUE;									// Dont ask for more data now!
	do
	{
		len = recv(vncc_sock, (char*)&vncc_rxbuf, sizeof(vncc_rxbuf), MSG_PEEK | MSG_DONTWAIT);
		//printf("peek len=%d\n",len);
		if (len>0)
		{
			len = recv(vncc_sock, (char*)&vncc_rxbuf, len, 0);
			ESP_LOGE(TAG,"from[%s] Throwing away %d bytes", s, len);
		}
	} while (len>0);
	vncc_busy = FALSE;
}




void vncc_process_rectangle(int r)
{
	struct		vnc_rect	rec;
	uint16_t	pixels[1024];
        uint32_t	sclock;										// start time
        uint32_t	eclock;										// end time
	uint32_t	tms;										// time in millliseconds
	int		len = 0;
	int		l   = 0;
	int		dw  = 0;									// display width
	int		dh  = 0; 									// display height

        sclock = (uint32_t)clock();									// Default clock is 10ms resolution
	vncc_busy = TRUE;
	len = readbytes(vncc_sock, (char*)&rec, sizeof(struct vnc_rect));				// Get VNC rectange header
	if (len==sizeof(struct vnc_rect))
	{
		rec.xpos		= bswap16(rec.xpos);
		rec.ypos		= bswap16(rec.ypos);
		rec.width		= bswap16(rec.width);
		rec.height		= bswap16(rec.height);
		rec.encoding_type	= bswap32(rec.encoding_type);

		// TODO: Check what last rectange really does and how to handle it
		if (rec.encoding_type==-1 || rec.xpos==65535 || rec.ypos==65535 || rec.height==65535)	// "LastRect", server is cutting list short
			return;

		printf("rect %05d xpos=%05d ypos=%05d width=%05d height=%05d et=%d ",r+1,
			rec.xpos, rec.ypos, rec.width, rec.height, rec.encoding_type);
		dw = jag_get_display_width();								// Check requested rectangle fits on display
		dh = jag_get_display_height();
		if (rec.width > dw || rec.height >dh || rec.xpos+rec.width >dw || rec.ypos+rec.height >dh)
		{
			ESP_LOGE(TAG,"vncc_process_rectangle() Rectange larger than or clips display dimensions %d x %d",dw,dh);
			vncc_drain("process_rectangle");
			return;
		}

		did_draw=TRUE;										// We did draw something on the LCD
		switch (rec.encoding_type)
		{
			// Read and process data one line at a time, we do not have enough RAM to read an entire framebuffer
			case VNC_ET_RAW:								// 0x0000
				for (l=0;l<rec.height;l++)						// for each line of the rectangle
				{
					readbytes(vncc_sock, (char*)&pixels, rec.width*2);		// read one lines worth of pixel data
					ets_delay_us(375);						// Bug in ESP drivers or hardware issue ?
					//jag_draw_icon(rec.xpos, rec.ypos+l, rec.width, 1, (char*)&pixels);
					jag_draw_bitmap(rec.xpos, rec.ypos+l, rec.width, 1, (uint16_t*)&pixels);
				}
			break;

			case VNC_ET_COPYRECT:
				ESP_LOGE(TAG,"VNC_ET_COPYRECT not implimented yet");
			break;
			case VNC_ET_RRE:
				ESP_LOGE(TAG,"VNC_ET_RRE not implimented yet");
			break;
				
			case VNC_ET_HEXTILE:
				ESP_LOGE(TAG,"VNC_ET_HEXTILE not implimented yet");
			break;

			case VNC_ET_TRLE:
				ESP_LOGE(TAG,"VNC_ET_TRLE not implimented yet");
				
			break;
				
			case VNC_ET_ZRLE:
				ESP_LOGE(TAG,"VNC_ET_ZRLE not implimented yet");
			break;

			default:
				ESP_LOGE(TAG,"Uknown encoding type %d %08X",rec.encoding_type, rec.encoding_type);
			break;
		}
        	eclock = (uint32_t)clock();
        	tms = (uint32_t) (eclock - sclock) * 1000 / CLOCKS_PER_SEC;
		printf("took %ums\n", tms);
	}
	else	ESP_LOGE(TAG,"vncc_process_rectangle() expected %d, got %d",sizeof(struct vnc_rect),len);
	vncc_busy = FALSE;
	return;
}


static void vncc_process_framebufferupdate()
{
	struct vnc_FramebufferUpdate		fbu;
	int    r=0;
	int    len=0;

	//vncc_busy = TRUE;
	len = readbytes(vncc_sock, (char*)&fbu, sizeof(struct vnc_FramebufferUpdate));
	if (len==sizeof(struct vnc_FramebufferUpdate))
	{
		fbu.num_of_rectangles	= bswap16(fbu.num_of_rectangles);
		printf("Got VNC_SMT_FRAMEBUFFERUPDATE %d rectangles\n",fbu.num_of_rectangles);
		if (fbu.num_of_rectangles==0)
			return;
		if (fbu.num_of_rectangles >2048)						// unlikely, not a 4k display
		{
			vncc_drain("process_framebufferupdate()");
			return;
		}

		for (r=0;r<fbu.num_of_rectangles;r++)						// N rectangles follow
			vncc_process_rectangle(r);						// read and process each one
	}
	else	ESP_LOGE(TAG,"vncc_process_framebufferupdate() expected %d read, got %d",sizeof(struct vnc_FramebufferUpdate),len);
	//vncc_busy = FALSE;
}



// Note: never been called hence untested
static void vncc_process_colormapentry()
{
	int len=0;
	struct vnc_colormapentry	cme;
	struct vnc_rgbentry		rgbe;
	int i=0;

	len = readbytes(vncc_sock, (char*)&cme, sizeof(struct vnc_colormapentry));
	if (len==sizeof(struct vnc_colormapentry))
	{
		cme.first_color		= bswap16(cme.first_color);
		cme.number_of_colors	= bswap16(cme.number_of_colors);
		printf("Got VNC_SMT_SETCOLORMAPENTRIES  %d RGB entries follow\n",cme.number_of_colors);

		for (i=0;i<cme.number_of_colors;i++)
		{
			len=readbytes(vncc_sock, (char*)&rgbe, sizeof(struct vnc_rgbentry));
			if (len!=sizeof(struct vnc_rgbentry))
				return;								// Socket probably hung up
			//printf("rgbe.red=%d\n",rgbe.red);
		}
	}
	else	ESP_LOGE(TAG,"vncc_process_colormapentry() expected %d read %d",sizeof(struct vnc_colormapentry), len);
}


static void vncc_process_servercuttext()
{
	struct vnc_servercuttext	sct;
	uint32_t i = 0;
	char	 b;

	readbytes(vncc_sock, (char*)&sct, sizeof(struct vnc_servercuttext));			// Get header
	sct.textlen = bswap32(sct.textlen);
	printf("Got VNC_SMT_SERVERCUTTEXT %d bytes\n",sct.textlen);
	for (i=0;i<sct.textlen;i++)
	{
		readbytes(vncc_sock, (char*)&b, 1);
		printf("%c",b);
	}	
	printf("\n");
}



// The VNC server and the client do not agree on display dimensions
void display_mismatch()
{
	int w = jag_get_display_width();
	int h = jag_get_display_height();
	char st[256];

	lcd_textbuf_printstring("Display mismatch, got: \n");
	sprintf(st,"%d x %d bpp=%d\n",vncc_si.fbwidth, vncc_si.fbheight, vncc_si.pf_depth);
	lcd_textbuf_printstring(st);
	if (vncc_si.fbwidth != w)
	{
		sprintf(st,"Got w=%d need %d\n", vncc_si.fbwidth, w);
		lcd_textbuf_printstring(st);
		ESP_LOGE(TAG,"%s",st);
	}

	if (vncc_si.fbheight != h)
	{
		sprintf(st,"Got h=%d need %d\n", vncc_si.fbheight, h);
		lcd_textbuf_printstring(st);
		ESP_LOGE(TAG,"%s",st);
	}

	if (vncc_si.pf_depth != 16)
	{
		sprintf(st,"Got bpp=%d need 16\n", vncc_si.pf_depth);
		lcd_textbuf_printstring(st);
		ESP_LOGE(TAG,"%s",st);
	}
}




// TODO:  Extract server major and minor verison, check we have a new enough version
static void vncc_client_task(void *pvParameters)
{
	int len=0;
	int err=0;
	uint8_t msg_type=0;

	while (1)
	{
		if (vncc_state!=VNCC_MAINLOOP)
			printf("vncc_state=%d\n",vncc_state);
		switch (vncc_state)
		{
			case VNCC_NOT_CONNECTED:
				vncc_busy = FALSE;
				if (vncc_sock<=0)						// No TCP connection yet ?
				{
					do							// then keep trying
					{
						vncc_doconnect();				// to connect
						if (vncc_sock<=0)
						{
							lcd_textbuf_printstring("Failed to connect\n");
							vTaskDelay(3000 / portTICK_PERIOD_MS);
						}
						else	
						{
							lcd_textbuf_printstring("Got connection\n");
							ESP_LOGI(TAG,"Got connection");
							vncc_state = VNCC_EXPECTING_GREETING;
						}
					} while (vncc_sock<=0);
				}
			break;



			case VNCC_EXPECTING_GREETING:
				bzero(&vncc_rxbuf,sizeof(vncc_rxbuf));
				len = recv(vncc_sock, vncc_rxbuf, sizeof(vncc_rxbuf) - 1, 0);	// -1, keep the last buffer byte a zero
				if (len!=12)
				{
					ESP_LOGE(TAG,"expected 12 bytes, got %d",len);
					vncc_shutdown();
				}
				if (strncmp((char*)&vncc_rxbuf,"RFB",3)==0)			// Greeting magic good ?
				{
					ESP_LOGI(TAG, "Successfully connected %s",vncc_rxbuf);
					sprintf(vncc_txbuf,"RFB 003.008\n");
					err = send(vncc_sock, (char*)&vncc_txbuf, 12, 0);	// Send my version
					if (err<0)	
						vncc_shutdown();
					vncc_state = VNCC_EXPECTING_NUM_SECURITY_TYPES;
				}
				else	
				{
					ESP_LOGE(TAG, "VNC greet bad magic");
					vncc_shutdown();
				}
			break;


			//TODO: Some actual authentication
			case VNCC_EXPECTING_NUM_SECURITY_TYPES:
				bzero(&vncc_rxbuf,sizeof(vncc_rxbuf));
				// Duno .. some bytes ...
				len = recv(vncc_sock, (char*)&vncc_rxbuf, sizeof(vncc_rxbuf), 0);
				printf("security types, got %d bytes\n",len);
				dumphex((char*)&vncc_rxbuf, len);
				vncc_txbuf[0]=1;						// Send my security type (1=NONE)
				err = send(vncc_sock, (char*)&vncc_txbuf, 1, 0);		
				vncc_state = VNCC_EXPECTING_SECURITY_RESULT;
			break;



			case VNCC_EXPECTING_SECURITY_RESULT:
				len = recv(vncc_sock, (char*)&vncc_rxbuf, 4, 0);		// Expecting "SecurityResult" (4) 
				if (len==4)
				{
					printf("security result (should be 00 00 00 00) = ");
					dumphex((char*)&vncc_rxbuf, 4);
					vncc_txbuf[0]=1;					// Send ClientInit (Shared)
					err = send(vncc_sock, (char*)&vncc_txbuf, 1, 0);		
					vncc_state = VNCC_EXPECTING_SERVER_INIT;
				}
				else	vncc_shutdown();
			break;


			case VNCC_EXPECTING_SERVER_INIT:
				vTaskDelay(1000 / portTICK_PERIOD_MS);
				len = recv(vncc_sock, (char*)&vncc_rxbuf, sizeof(struct vnc_ServerInit), 0);
				printf("read server init, got %d bytes, expecting %d\n",len,sizeof(struct vnc_ServerInit));
				dumphex((char*)&vncc_rxbuf, len);
				if (len==sizeof(struct vnc_ServerInit))
					process_server_init((struct vnc_ServerInit*)&vncc_rxbuf);
				else	vncc_shutdown();
			//JA new, test
				if (vncc_si.fbwidth != jag_get_display_width() || vncc_si.fbheight != jag_get_display_height() || vncc_si.pf_depth != 16)
				{
					display_mismatch();
					vncc_shutdown();
					vTaskDelay(8000 / portTICK_PERIOD_MS);
				}
				else
				{
					vncc_send_setencodings(); 
					lcd_textbuf_enable(FALSE, FALSE);			// Make sure task stops driving SPI LCD
					vncc_send_framebuffer_update_request(0, 0, 240, 320, 0);	// ASk for entire screen now
					vncc_state = VNCC_MAINLOOP;
				}
			break;


			case VNCC_MAINLOOP:
				len=readbytes(vncc_sock, (char*)&msg_type,1);
				//printf("msg_type=%02X (%d)\n",msg_type,msg_type); fflush(stdout);
				switch (msg_type)
				{
					case VNC_SMT_FRAMEBUFFERUPDATE:				// 0
						vncc_process_framebufferupdate();
					break;

					case VNC_SMT_SETCOLORMAPENTRIES:			// 1
						vncc_process_colormapentry();
					break;
	
					case VNC_SMT_BELL:					// 2
						ESP_LOGE(TAG,"VNC_SMT_BELL Not implimented yet");
					break;
	
					case VNC_SMT_SERVERCUTTEXT:				// 3
						vncc_process_servercuttext();
					break;

					default:
						ESP_LOGE(TAG,"got msg_type %02X ?",msg_type);
						vncc_drain("mainloop");
					break;
				}
			break;
		}
		vTaskDelay(5 / portTICK_PERIOD_MS);
	}
}



// socket reads are blocking, so best to do the requests on a task of its own
static void vncc_periodic_request_and_touch_task(void *pvParameters)
{
	touch_panel_points_t    points;
	uint32_t 		x=0;
	uint32_t		y=0;
	uint32_t		e=0;
	static int		px=0;
	static int		py=0;
	static int		pe=0;

	while (1)
	{
		if (vncc_state==VNCC_MAINLOOP && vncc_sock >0)
		{
			if (vncc_busy!=TRUE)							// Connected and otherwise idle
				vncc_send_framebuffer_update_request(0, 0, 240, 320, 1);	// Ask for rectangles (incremental)

			touch_drv.read_point_data(&points);
			x=points.curx[0];
			y=points.cury[0];
			e=points.event;
			if (x!=px || y!=py || e!=pe)						// cursor changed ?
			{
				if (e == TOUCH_EVT_PRESS)
				{
					vncc_send_pointer_event((uint16_t)x, (uint16_t)y, 1);	// mouse push down
					vncc_send_pointer_event((uint16_t)x, (uint16_t)y, 0);	// mouse release
				}
				px=x;
				py=y;
				pe=e;
			}
		}
		vTaskDelay((1000/vncc_update_rate_hz) / portTICK_PERIOD_MS);			// limit request rate to N Hz
	}
}



void vncc_connect(char *host_ip, int screennum)
{
	if (vncc_sock >0 && vncc_taskcreated==TRUE)						// already connected ?
		vncc_shutdown();								// then hang up now

	strncpy(vncc_host_ip, host_ip, sizeof(vncc_host_ip));
	vncc_screennum=screennum;
	if (vncc_taskcreated!=TRUE)
	{
		xTaskCreate(vncc_client_task, "vnc_task", 20*1024, NULL, configMAX_PRIORITIES -1 , NULL);
		xTaskCreate(vncc_periodic_request_and_touch_task, "req_task", 8*1024, NULL, 5, NULL);
	}
}



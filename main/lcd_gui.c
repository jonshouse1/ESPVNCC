/* 
	lcd_gui.c

	Great care needs to be taken so that only one task at a time is writing to the 
	LCD display, otherwise we will get display corruption
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


//JA
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
//#include "lwip/sockets.h"
#include <time.h>

// PWM for backlight
#include "driver/ledc.h"

// LCD and touch screen
#include "screen_driver.h"
#include "touch_panel.h"


#include "global.h"
#include "lcd_textbuf.h"
#include "lcd_ts_init.h"
#include "lcd_vncc.h"
#include "jag.h"


// extern globals
extern scr_driver_t		lcd_drv;
extern touch_panel_driver_t	touch_drv;

extern const char *TAG;
extern int 		online;
extern char 		client_ip[16];
extern char 		server_ip[16];
extern int  		link_up;
extern int 		connection_state;
//extern int 		doredraw;

// LCD gui state
extern int 		backlight;
extern int 		gui_screen;	
extern int		pgui_screen;

int			dtchanged=FALSE;
int			pbacklight=-1;

uint32_t		push_durationms[4];
uint32_t		push_lastpushtime[4];
int			push_state[4];




// Check if bot left of display being pushed
int lcd_screen_check_bot_held(uint16_t x, uint16_t y, uint16_t e)
{
	uint32_t		clock_ms;
	static uint32_t		blp_last_push_time=0;
	static uint32_t		blp_push_durationms=0;

	clock_ms = (uint32_t) (clock() * 1000 / CLOCKS_PER_SEC);
	if (e == TOUCH_EVT_PRESS)
	{
		//if (y>280 && x<40)
		if (y>280)
		{
			if (blp_last_push_time>0)
				blp_push_durationms = blp_push_durationms + (clock_ms - blp_last_push_time);
			else	blp_push_durationms = 0;
			blp_last_push_time = clock_ms;
			printf("blp_push_durationms=%d\n",blp_push_durationms);
			if (blp_push_durationms>2000)
			{
				blp_push_durationms=0;
				blp_last_push_time=0;
				if (x<40)
					return(1);		// bot left held
				if (x>200)
					return(2);		// bot right held
			}
		}
	}
	else	blp_last_push_time=0;
	return(0);
}




void lcd_task(void *pvParameters)
{
	touch_panel_points_t	points;
	char st[32];
	int			b=0;
	int32_t			x=0, 	y=0;
	int32_t			px=0,	py=0;
	uint16_t		e=0,	pe=0;
	//int			redraw=TRUE;
	uint16_t		i=0;
	//uint32_t		clock_ms=0;
	

	connection_state=1;
	while(1) 
	{
		//clock_ms = (uint32_t) (clock() * 1000 / CLOCKS_PER_SEC);
		// Read touch panel for all "screens"
		touch_drv.read_point_data(&points);
		x=points.curx[0];
		y=points.cury[0];
		e=points.event;

/*
		if (doredraw>0)							// Another process asked for a redraw sometime soon
		{
			doredraw--;
			if (doredraw<=0)
			{
				redraw=TRUE;
				pgui_screen=-111;
				doredraw=0;
			}
		}
*/

		if (gui_screen != pgui_screen)						// changing screens displayed ?
		{
			//redraw=TRUE;							// screen changed, need complete redraw
			ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
			switch (gui_screen)						// which screen are we swithing to ?
			{
				case JLC_GUI_SCREEN_TEXTBUF:
					ESP_LOGI(TAG,"Switching to JLC_GUI_SCREEN_TEXTBUF");
					lcd_textbuf_enable(TRUE, TRUE);			// start text console back up
				break;

				case JLC_GUI_SCREEN_VNC:
					ESP_LOGI(TAG,"Switching to JLC_GUI_SCREEN_VNC");
					lcd_textbuf_enable(FALSE, FALSE);		// Make sure task stops driving SPI LCD
				break;

				case JLC_GUI_SCREEN_DISPLAYTEST:
					ESP_LOGI(TAG,"Switching to JLC_GUI_SCREEN_DISPLAYTEST");
					lcd_textbuf_enable(FALSE, FALSE);		// Make sure task stops driving SPI LCD
					jag_cls(COLOR_BLACK);
					i=0;
				break;
					
			}
			pgui_screen = gui_screen;
		}


		switch (gui_screen)							// render whatever screen is active
		{
			case JLC_GUI_SCREEN_TEXTBUF:
				vTaskDelay(4);			// Allow more time for the other tasks updating the text screen
			break;

			case JLC_GUI_SCREEN_VNC:
				if (x!=px || y!=py || e!=pe)				// cursor changed ?
				{
					if (e == TOUCH_EVT_PRESS)
					{
						vncc_send_pointer_event((uint16_t)x, (uint16_t)y, 1);		// mouse push down
						vncc_send_pointer_event((uint16_t)x, (uint16_t)y, 0);		// mouse release
					}
					px=x;
					py=y;
					pe=e;
				}
			break;

			case JLC_GUI_SCREEN_DISPLAYTEST:
				jag_fill_lines(64*0 , 64, COLOR_RED);
				jag_fill_lines(64*1 , 64, COLOR_GREEN);
				jag_fill_lines(64*2 , 64, COLOR_BLUE);
				sprintf(st,"%u",i);
				jag_draw_string(100, 64*2+42, (char*)&st, &Font16, COLOR_BLUE, COLOR_WHITE);
				i++;
				jag_fill_lines(64*3 , 64, COLOR_YELLOW);
				jag_fill_lines(64*4 , 64, COLOR_WHITE);
			break;
		}


		// Holding bottom left of touch screen toggles forms
		b=lcd_screen_check_bot_held(x, y, e);					// Bot of sceen was held down
		if (b>0)								// something was held down
		{
			switch (b)
			{
				case 1:							// bot left held down
					switch (gui_screen)
					{
						case JLC_GUI_SCREEN_TEXTBUF:	    gui_screen = JLC_GUI_SCREEN_VNC;  break;
						case JLC_GUI_SCREEN_VNC: 	    gui_screen = JLC_GUI_SCREEN_TEXTBUF;   break;
						case JLC_GUI_SCREEN_DISPLAYTEST:    gui_screen = JLC_GUI_SCREEN_VNC;  break;
					}
				break;

				case 2:							// bot right held down
					switch (gui_screen)
					{
						case JLC_GUI_SCREEN_TEXTBUF:        gui_screen = JLC_GUI_SCREEN_DISPLAYTEST;  break;
						case JLC_GUI_SCREEN_VNC:            gui_screen = JLC_GUI_SCREEN_DISPLAYTEST;   break;
						case JLC_GUI_SCREEN_DISPLAYTEST:    gui_screen = JLC_GUI_SCREEN_VNC;  break;
					}
				break;
			}
		}

		if (backlight != pbacklight)
		{
			led_pwm_set(backlight);
			pbacklight = backlight;
		}
		vTaskDelay(10);
	}
}






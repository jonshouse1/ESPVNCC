/* 
	jag.c

	Version  0.5
	Last modified:
	22 Jul 2021

	J A Graphics lib for ESP32 IDF (Rtos)
	Assumes 16 bit display only

	An attempt to make a thread safe graphics library

	Every LCD write should go through this code.

	painter_fonts are part of the esp-iot-solution touch screen calibration code.
	The calibration code is linked in with the lcd driver support.

	This code drives the esp-iot-solution LCD driver functions found here:
	esp/esp-iot-solution/components/display/screen/controller_driver/
*/


#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"

#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"

#include "screen_driver.h"
#include "touch_panel.h"
#include "jag.h"
#include "painter_fonts.h"

#define JAG_MAXPIXELS_PERLINE	1200						// the maximum number of pixels for one displayed line


extern const char *TAG;
static scr_driver_t		jag_lcd_drv;
static int 			jag_lock	= FALSE;			// one global lock, TRUE if lock is on
static uint16_t			jag_lines	= 0;
static uint16_t			jag_cols	= 0;



// ESP32 has more than one core, the ESP idf will starts tasks on multiple cores simultaneously. If multiple processes
// are calling in the lck_wl will be counting up.
static uint32_t			lck_wl=0;					// Atomic type for ESP32
static uint32_t			lck_gl=0;
static uint32_t			lck_rl=0;


// Am I skilled enough to write these as macros, probably not. Maybe I should use the freertos mutexes instead one day
static inline void __attribute__((always_inline)) jag_grab_lock()
{
grrr:
	while (jag_lock == TRUE)						// wait for the lock
	{
		vTaskDelay(1);							// in the schedular
		lck_wl++;
	}
	if (jag_lock==TRUE)							// check nothing snuck in
		goto grrr;							// if they did start over again
	jag_lock = TRUE;							// now we have the lock
	lck_gl++;
}


static inline void __attribute__((always_inline)) jag_release_lock()
{
	if (jag_lock!=TRUE)							// if the lock was not on
	{
		while(1)							// then crash, something is out of sequence
			printf("jag_release_lock() The lock was not on\n");
	}
	jag_lock = FALSE;
	lck_rl++;
}



// Shadow==TRUE, keep a copy of the LCD frame and refresh it from time to time
void jag_init(scr_driver_t* driver, int cols, int lines)
{
        jag_lcd_drv     = *driver;
	jag_lines	= lines;
	jag_cols	= cols;	

	ESP_LOGI(TAG,"jag_init() - lines=%d cols=%d",lines,cols);
	scr_info_t lcd_info;
	jag_lcd_drv.get_info(&lcd_info);
	ESP_LOGI(TAG,"jag_init() - Screen name:%s | width:%d | height:%d", lcd_info.name, lcd_info.width, lcd_info.height);
}




// Everything comes through here! good place to serialise everything with a lock
void jag_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
	esp_err_t	ret;

	jag_grab_lock();
	ret=jag_lcd_drv.draw_bitmap(x, y, w, h, bitmap);			// Call ili9341 driver, limited to 4000ish bytes
	if (ret!=ESP_OK)							// set_window failed and no data was written
	{
		ESP_LOGE(TAG,"draw_bitmap returned %d",ret);
	}
	jag_release_lock();
}




// online line at a time, stack buffer, probably thread safe
void jag_draw_icon_s2(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *image)
{
	uint16_t	buf[JAG_MAXPIXELS_PERLINE];				// stack allocated buffer, thread safe 
	uint16_t	l   = 0;
	uint16_t	bytesperline=0;

	bytesperline = w*sizeof(uint16_t);
	for (l=0;l<h;l++)							// for every line of image
	{
		memcpy(&buf, image+(bytesperline*l), bytesperline);		// copy one line of pixels from flash to RAM
		jag_draw_bitmap(x, y+l, w, 1, (uint16_t*)&buf);			// draw line of pixels
	}
}

// Entire image at once, draw icon using malloc, thread safe but uses an image worth of RAM
void jag_draw_icon_ma(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *image)
{
	uint16_t*	buf = malloc(w*h*sizeof(uint16_t));			// malloc enough ram for entire image
	uint16_t	l   = 0;

	memcpy(buf, image, w*h*sizeof(uint16_t));				// entire image
	for (l=0;l<h;l++)							// for every line of image
		jag_draw_bitmap(x, y+l, w, 1, (uint16_t*)buf+(w*l));		// draw line of pixels
	free(buf);
}


// The basic draw_bitmap function only supports upto 4000 ish bytes written in one go.
// draw images one scan line at a time so we can work with any size of image
// across, down, width, height, pointer to rgb565 pixels
void jag_draw_icon(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *image)
{
ets_delay_us(320);
	//printf("jag_draw_icon() %d %d %d %d %p\n",x,y,w,h,image);
	jag_draw_icon_s2(x, y, w, h, image);
	//jag_draw_icon_ma(x, y, w, h, image);
	//printf("GL:%u\tRL:%u\tWL:%u\n",lck_gl,lck_rl,lck_wl); 
}




// Partially clear display, or just write N lines a color
void jag_fill_lines(uint16_t startline, uint16_t numlines, uint16_t color)
{
	uint16_t	buf[JAG_MAXPIXELS_PERLINE];
	uint16_t l=0;

	for (l=0;l<jag_cols;l++)
		buf[l]=color;
	for (l=startline;l<startline+numlines;l++)
		jag_draw_bitmap(0, l, jag_cols, 1, (uint16_t*)&buf);	
}



// Fill a rectange in a color
void jag_fill_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
}



// Clear entire display to a color
void jag_cls(uint16_t color)
{
	ESP_LOGI(TAG,"clearing %d lines of %d pixels",jag_lines,jag_cols);
	jag_fill_lines(0, jag_lines, color); 
}




// This routine is hammered and may be re-interant
void jag_draw_char(uint16_t x, uint16_t y, char ascii_char, const font_t *font, uint16_t bgcolor, uint16_t fgcolor)
{
	uint16_t buf[MAXCHARBUF];
	int i, j;
	uint16_t char_size = font->Height * (font->Width / 8 + (font->Width % 8 ? 1 : 0));
	unsigned int char_offset = (ascii_char - ' ') * char_size;
	const unsigned char *ptr = &font->table[char_offset];
	int ox=0;
	int oy=0;

	if (font==NULL)
		return;

	for (i=0;i<font->Width*font->Height;i++)
		buf[i]=bgcolor;
	for (j = 0; j < char_size; j++) 
	{
		uint8_t temp = ptr[j];
		for (i = 0; i < 8; i++)
		{
			if (temp & 0x80) 
				buf[ox+(font->Width*oy)]=fgcolor;

			temp <<= 1;
			ox++;
			if (ox == font->Width) 
			{
				ox=0;
				oy++;
				break;
			}
		}
	}
	jag_draw_bitmap(x, y, (uint16_t)font->Width, (uint16_t)font->Height, (uint16_t*)&buf);		// Draw NxN char
}



void jag_draw_string(uint16_t x, uint16_t y, char* text, const font_t *font, uint16_t bgcolor, uint16_t fgcolor)
{
	uint16_t i=0;

	for (i=0;i<strlen(text);i++)
		jag_draw_char(x+(font->Width * i), y, text[i], font, bgcolor, fgcolor);
}



// y is the line, x is the cener point of the text
void jag_draw_string_centered(uint16_t x, uint16_t y, char* text, const font_t *font, uint16_t bgcolor, uint16_t fgcolor)
{
	uint16_t i=0;
	uint16_t lp=0;						// left point

	//lp=x-(font->Width * (strlen(text)/2));
	lp=x-((strlen(text)*font->Width)/2);
	for (i=0;i<strlen(text);i++)
		jag_draw_char(lp+(font->Width * i), y, text[i], font, bgcolor, fgcolor);
}





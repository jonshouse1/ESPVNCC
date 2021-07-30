/* 
 * lcd_textbuf.c
 * Text display for ESP32, esp-idf
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


/* 
	lcd_textbuf.c
	Text framebuffer for ESP IDF

	See /esp/esp-iot-solution/components/display/touch_panel/calibration/
	basic_painter/fonts/
	font12.c  font16.c  font20.c  font24.c  font8.c  painter_fonts.h

	For Portrait 240 Wide, 320 Heigh
	Font		Lines		Cols	
	------------------------------------
	Font8		28		37
	Font12		23		26
	Font16		17		26

	Initial call order
		lcd_textbuf_init();
			Possibly accepting text via lcd_textbuf_printstring()
			but not driving LCD yet
		lcd_textbuf_setcolors();
			Set colours
		lcd_textbuf_enable(TRUE, TRUE);
			Clear display, start cursor going
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "lwip/sockets.h"


#include "global.h"
#include "lcd_textbuf.h"
#include "jag.h"


// Maximum size of buffer to hold one character of pixels in largest font
#define MAXCHARPIXELBUF		18*25*sizeof(uint16_t)
extern const char *TAG;


// Display terminal text
static		scr_driver_t		lcd_drv;
char 		textbuf[TEXTBUF_MAXLINES][TEXTBUF_MAXLINELEN];
char		ptextbuf[TEXTBUF_MAXLINES][TEXTBUF_MAXLINELEN];
int		curposl		= 0;
int		curposc		= 0;
static int	textbuf_enable	= FALSE;						// TRUE draws pixels to the LCD
static int	textbuf_redraw	= TRUE;							// TRUE redraws every character
uint16_t	textbuf_fgcolor = COLOR_GREEN;
uint16_t	textbuf_bgcolor = COLOR_BLACK; 

// Text
const font_t		*textbuf_font = NULL;
int			textbuf_lines	= 0;						// lcd_textbuf_init() populates these
int			textbuf_cols	= 0;

// Task state
BaseType_t		ctask		= pdFALSE;					// pdPASS if cursor task been created
TaskHandle_t 		xhandle;




void lcd_textbuf_clear(int t, int p)
{
	int l=0;
	int c=0;

	for (l=0;l<textbuf_lines;l++)
	{
		for (c=0;c<textbuf_cols;c++)
		{
			if (t==TRUE)
				textbuf[l][c]=' ';
			if (p==TRUE)
				ptextbuf[l][c]=0;					// never printed
		}
	}
}




// Draw a block cursor if co is TRUE. This called only from the timer task so is not re-enterant hence no locks
void lcd_textbuf_cursor()
{
	static int co=1;
	static uint16_t buf[MAXCHARPIXELBUF];
	uint16_t x=(textbuf_font->Width+2)*curposc;
	uint16_t y=(textbuf_font->Height+2)*curposl;
	uint16_t w=textbuf_font->Width;
	uint16_t h=textbuf_font->Height;
	int l=0;

	if (textbuf_enable != TRUE)
		return;
	for (l=0;l<w*h;l++)
	{
		if (co==TRUE)								// cursor should be displayed now ?
			buf[l]=textbuf_fgcolor;						// then render in foreground color
		else	buf[l]=textbuf_bgcolor;						// else render in background color
	}
	jag_draw_bitmap(x, y, w, h, (uint16_t*)&buf);					// Draw cursor
	co=!co;										// toggle on/off
}




// As font rendering is so slow render only characters that have changed 
// keep the text array even if textbuf_enable is not TRUE, but only write to LCD if textbuf_enable is TRUE
void lcd_textbuf_display()
{
	int l=0;
	int c=0;

	for (l=0;l<textbuf_lines;l++)
	{
		for (c=0;c<textbuf_cols;c++)
		{
			if (textbuf[l][c] != ptextbuf[l][c] || textbuf_redraw==TRUE)
			{
				if (textbuf[l][c]>=' ' && textbuf_enable==TRUE)		// Valud ASCII for our font ?
				{
					jag_draw_char((textbuf_font->Width+2)*c, (textbuf_font->Height+2)*l, 
							textbuf[l][c], textbuf_font, textbuf_bgcolor, textbuf_fgcolor);
				}
				// redraw on then write an ASCII space
				if (textbuf[l][c]<' ' && textbuf_redraw==TRUE && textbuf_enable==TRUE)
				{
					jag_draw_char((textbuf_font->Width+2)*c, (textbuf_font->Height+2)*l, 
							' ', textbuf_font, textbuf_bgcolor, textbuf_fgcolor);
				}
			}
			ptextbuf[l][c] = textbuf[l][c];
		}
	}
	textbuf_redraw=FALSE;
}



// update the text array (character buffer)
// this may be called before lcd_textbuf_init(), but this is ok as this does not try and render any pixels
// function may be called by simultanious tasks so needs to be 'thread safe'
void lcd_textbuf_printstring(char *st)
{
	int i=0;
	int l=0;
	int c=0;

	for (i=0;i<strlen(st);i++)							// for every character
	{
		if (st[i]=='\n')							// starting a new line ?
		{
			curposc=0;							// start on the left
			curposl++;							// of the next line down
		}
		else
		{
			textbuf[curposl][curposc]=st[i];				// copy character into text buffer
			curposc++;							// keep moving cursor right
		}
		if (curposc>=textbuf_cols)						// until cursor hits line end
		{
			curposc=0;							// start on the left
			curposl++;							// of the next line down
		}
		if (curposl>=textbuf_lines)						// hit end of screen, then scroll
		{
			for (l=1;l<textbuf_lines;l++)					// starting at second line, finishing on last
			{
				for (c=0;c<textbuf_cols;c++)				// for every char of a line
				{
					textbuf[l-1][c]=textbuf[l][c];			// move line up one	
					ptextbuf[l-1][c]=0;				// ensure char gets re-rendered
				}
			}
			for (c=0;c<textbuf_cols;c++)
				textbuf[textbuf_lines-1][c]=' ';			// blank bottom line
			lcd_textbuf_clear(FALSE,TRUE);					// redraw all chars on next textbuf_update()
			curposl=textbuf_lines-1;					// Last line
			curposc=0;							// start on the left
			textbuf_redraw=TRUE;
		}
	}
	// Calling textbuf_display causes jag.c to use its locks for real, on balance it just seems to make it all slower
	//lcd_textbuf_display();
}



void lcd_textbuf_set_cursor_position(int l, int c)
{
	curposl=l;
	curposc=c;
	if (curposc>textbuf_cols)
		curposc=textbuf_cols;
	if (curposl>textbuf_lines)
		curposl=textbuf_lines;
}



// e = TRUE = enabled, updates LCD,  FALSE=disabled, no LCD updates
void lcd_textbuf_enable(int e, int cleardisplay)
{
	if (cleardisplay==TRUE)
	{
		textbuf_enable	= FALSE;
		vTaskDelay(80);
		jag_cls(textbuf_bgcolor);
		textbuf_redraw	= TRUE;							// re-draw all the text
	}
	if (e==TRUE)									// starting ...
	{
		textbuf_enable	= TRUE;							// start drawing to display
		textbuf_redraw	= TRUE;							// re-draw all the text
		vTaskResume(xhandle);							// startup cursor and rendering
	}
	else										// enable is false	
	{
		textbuf_enable	= FALSE;						// stop drawing to display
		vTaskDelay(80);
		vTaskSuspend(xhandle);							// stop cursor and rendering
	}
}


// task stays resident after init, update the cursor and render text buffer to graphics if enabled
void textbuf_task()
{
	int	x=0;
	while (1)
	{
		lcd_textbuf_display();
		x++;
		if (x>3)
		{
			if (textbuf_redraw!=TRUE)					// display needs updating
				lcd_textbuf_cursor();					// so skip cursor this time round
			x=0;
		}
		if (textbuf_redraw!=TRUE)						// dont yeild if scrolling
			vTaskDelay(8);							// Cursor blink rate
	}
}


// Called after init
void lcd_textbuf_setcolors(uint16_t fgcolor, uint16_t bgcolor)
{
	textbuf_fgcolor = fgcolor;
	textbuf_bgcolor = bgcolor; 
}



// Caller tells us how many lines of text and how many characters per line
void lcd_textbuf_init(scr_driver_t* driver, const font_t* font, int lines, int cols)
{
	curposl		= 0;
	curposc		= 0;
	lcd_drv 	= *driver;
	textbuf_font	= font;
	textbuf_lines	= lines;							// lcd_textbuf_init() populates these
	textbuf_cols	= cols;
	textbuf_redraw	= TRUE;	

	//scr_info_t lcd_info;
	//lcd_drv.get_info(&lcd_info);
	//ESP_LOGI(TAG,"Screen name:%s | width:%d | height:%d", lcd_info.name, lcd_info.width, lcd_info.height);

	if (ctask != pdPASS)								// not already created ?
		ctask = xTaskCreate(textbuf_task, "textbuf_cursor_task", 16*1024, NULL, 3, &xhandle);  

	ESP_LOGI(TAG,"lcd_textbuf_init() - LCD text terminal %d lines x %d cols, each %dx%d",
		lines, cols, textbuf_font->Width, textbuf_font->Height);
}



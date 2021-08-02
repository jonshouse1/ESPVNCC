/* 
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


#include "screen_driver.h"
#include "painter_fonts.h"

#define TRUE			1
#define FALSE			0
#define TEXTBUF_MAXLINES	40				// MAX number of lines for text buffer
#define TEXTBUF_MAXLINELEN	120				// MAX number of characters per line for text buffer


// Prototypes
//void lcd_textbuf_init(const font_t* font);
void lcd_textbuf_init(const font_t* font, int ox, int oy, int forcelines, int forcecols);
void lcd_textbuf_clear(int t, int p);
void lcd_textbuf_setcolors(uint16_t fgcolor, uint16_t bgcolor);
void lcd_textbuf_enable(int e, int cleardisplay);
void lcd_textbuf_printstring(char *st);
void lcd_textbuf_set_cursor_position(int l, int c);
int lcd_textbuf_getlines();
int lcd_textbuf_getcols();





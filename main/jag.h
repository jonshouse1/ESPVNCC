/* 
	jag.h
	J A Graphics lib for ESP32 IDF (Rtos)
*/


#include "painter_fonts.h"


#define MAXCHARBUF        18*25*sizeof(uint16_t)                          // Maximum size of buffer to hold one character of dots in largest font
#define TRUE                    1
#define FALSE                   0


void jag_init(scr_driver_t* driver, int cols, int lines);
void jag_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
void jag_draw_icon(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *image);
void jag_draw_iconm(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *image);
void jag_fill_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void jag_fill_lines(uint16_t startline, uint16_t numlines, uint16_t color);
void jag_cls(uint16_t color);
void jag_draw_char(uint16_t x, uint16_t y, char ascii_char, const font_t *font, uint16_t bgcolor, uint16_t fgcolor);
void jag_draw_string(uint16_t x, uint16_t y, char* text, const font_t *font, uint16_t bgcolor, uint16_t fgcolor);
void jag_draw_string_centered(uint16_t x, uint16_t y, char* text, const font_t *font, uint16_t bgcolor, uint16_t fgcolor);




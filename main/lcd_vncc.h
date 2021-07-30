/* lcd_vncc.h
 *
 * Copyright (c) 2021 Jonathan Andrews. All rights reserved.
 * This file is part of the VNC client for Arduino.
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

//  State machine

#define VNCC_NOT_CONNECTED			0
#define VNCC_EXPECTING_GREETING			1
#define VNCC_EXPECTING_NUM_SECURITY_TYPES	2
#define VNCC_EXPECTING_SECURITY_TYPE		3
#define VNCC_EXPECTING_SECURITY_RESULT		4
#define VNCC_EXPECTING_SERVER_INIT		5
#define VNCC_MAINLOOP				10



// See RFC 6143

// Client message type 
#define VNC_CMT_SETPIXELFORMAT			0
#define VNC_CMT_SETENCODINGS			2
#define VNC_CMT_FRMAEBUFFERUPDATEREQUEST	3
#define VNC_CMT_KEYEVENT			4
#define VNC_CPOINTEREVENT			5
#define VNC_CLIENTCUTTEXT			6

// Server message type 
#define VNC_SMT_FRAMEBUFFERUPDATE		0
#define VNC_SMT_SETCOLORMAPENTRIES		1
#define VNC_SMT_BELL				2
#define VNC_SMT_SERVERCUTTEXT			3

// Encoding types
#define VNC_ET_RAW				0
#define VNC_ET_COPYRECT				1
#define VNC_ET_RRE				2
#define VNC_ET_HEXTILE				5
#define VNC_ET_TRLE				15
#define VNC_ET_ZRLE				16


struct __attribute__ ((__packed__)) vnc_servercuttext
{
	uint8_t		padding[3];
	uint32_t	textlen;
};

struct __attribute__ ((__packed__)) vnc_ServerInit
{
	uint16_t	fbwidth;
	uint16_t	fbheight;
	uint8_t		pf_bpp;
	uint8_t		pf_depth;
	uint8_t		pf_bigendian;
	uint8_t		pf_truecolor;
	uint16_t	pf_maxred;
	uint16_t	pf_maxgreen;
	uint16_t	pf_maxblue;
	uint8_t		pf_shiftred;
	uint8_t		pf_shiftgreen;
	uint8_t		pf_shiftblue;
	uint8_t		pf_padding[3];	
	uint32_t	namelen;
};


struct __attribute__ ((__packed__)) vnc_PointerEvent
{
	uint8_t			msg_type;
	uint8_t			button_mask;		// Mask for 8 button, 1=down
	uint16_t		xpos;
	uint16_t		ypos;	
};

struct __attribute__ ((__packed__)) vnc_FramebufferUpdateRequest
{
	uint8_t		msg_type;
	uint8_t		increm;
	uint16_t	xpos;
	uint16_t	ypos;
	uint16_t	width;
	uint16_t	height;
};


struct __attribute__ ((__packed__)) vnc_FramebufferUpdate
{
	uint8_t		padding;
	uint16_t	num_of_rectangles;
};


struct __attribute__ ((__packed__)) vnc_rect
{
	uint16_t	xpos;
	uint16_t	ypos;
	uint16_t	width;
	uint16_t	height;
	//uint32_t	encoding_type;			// Should be signed
	int32_t		encoding_type;			// Should be signed
};


struct __attribute__ ((__packed__)) vnc_colormapentry
{
	//uint8_t		 message-type     
	uint8_t			padding;
	uint16_t		first_color;
	uint16_t		number_of_colors;
};


struct __attribute__ ((__packed__)) vnc_rgbentry
{
	uint16_t		red;
	uint16_t		green;
	uint16_t		blue;
};



// Prototypes
void vncc_connect(char* hostname, int screennum);
void vncc_shutdown();
void vncc_send_pointer_event(uint16_t x, uint16_t y, uint8_t msk);



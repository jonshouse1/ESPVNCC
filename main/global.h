// Prototypes and global header


// The currently displayed form
#define JLC_GUI_SCREEN_GROUP_SWITCHES	2
#define JLC_GUI_SCREEN_TEXTBUF		3
#define JLC_GUI_SCREEN_DISPLAYTEST	10
#define JLC_GUI_SCREEN_VNC		20


#define JLC_MAX_GROUP_SWITCHES	4


// Prototypes
char* printuid(char unsigned *uid);
int os_printf(char *fmt, ...);
//void do_jcp_registrations();
void yafdp_server_task(void *pvParameters);
void jcp_client_task(void *pvParameters);
int udp_generic_send(char *d, int len, char *destination_ip, int destination_port, int broadcast);
void DumpHex(const void* data, size_t size);
//void jcp_send_device_state(int idx, uint16_t value1, uint16_t value2, int asciiorbinary, char* valuebytes, int valuebyteslen);
void lcd_gui_datetimestring(int tlen, char *tdata);


//void lcd_textbuf_clear();
//void lcd_textbuf_init();
//void lcd_textbuf_printstring(char *st);
//void lcd_gotolinecol(int l, int c);
//int lcd_printf(char *fmt, ...);

void sensor_i2c_sht30_init();

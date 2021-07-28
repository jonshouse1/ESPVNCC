/*
 	os_printf.c
*/


#include <stdio.h>
#include <string.h>
#include "global.h"

#include "esp_log.h"		// ESP IDF ESP_LOGI, ESP_LOGE 


extern const char *TAG;


#ifndef ESP8266
#include <stdarg.h>
int os_printf(char *fmt, ...)
{
        va_list arg;
        char buf[8192];

        va_start(arg, fmt);
        buf[0]=0;
        vsprintf((char*)&buf, fmt, arg);
        if (strlen(buf)>0)
        {
                //printf("%s",buf);
		if (strlen(buf)>1)
			buf[strlen(buf)-1]=0;
		ESP_LOGI(TAG,"%s",buf);
        }
        va_end(arg);
        fflush(stdout);
        return(0);
}
#endif




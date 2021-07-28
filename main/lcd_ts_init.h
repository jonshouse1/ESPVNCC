/* 
	lcd_ts_init.h
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
#include "driver/gpio.h"
#include "sdkconfig.h"


//JA
//#include "esp_freertos_hooks.h"
//#include "freertos/semphr.h"
#include "esp_system.h"

// PWM for backlight
#include "driver/ledc.h"

// LCD and touch screen
#include "screen_driver.h"
#include "touch_panel.h"




void led_pwm_set(int b);
void lcd_init();



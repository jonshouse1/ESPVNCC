/* 
	lcd_ts_init.c

	Initialise LCD and Touch screen
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
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
//#include "lwip/sockets.h"

// PWM for backlight
#include "driver/ledc.h"

// LCD and touch screen
#include "screen_driver.h"
#include "touch_panel.h"


//#include "lcd_ts_init.h"
//#include "global.h"
//#include "lcd_text_terminal.h"


// Dont use GPIO1(TX) and GPIO3(RX)
// GPIO34,35,36,39 input only
// SPI Max speed is 26.7Mhz if you wish to read data via SPI as well as write it
// Touch screen controller does not state max speed, all application examples are 2Mhz though 


// SPI Speed
//	use 26700000 or less if you need to read back from the display
//	32000000 is the fastest that works so far.
//

//#define SPI_SPEED_LCD_HZ	26700000	
#define SPI_SPEED_LCD_HZ	32000000	
#define SPI_SPEED_TOUCH_HZ	4000000
#define GPIO_LCDBL		4							// LCD Back light LED 
#define GPIO_TCS		3							// GPIO Touch screen Chip select
#define GPIO_LCDCS		32	
#define GPIO_LCDRESET		2
//#define GPIO_TIRQ		36							// Touch screen interrupt
#define GPIO_TIRQ		-1							// Touch screen interrupt
#define GPIO_DATACMD		5							// LCD Data/RS (data/command) 
#define GPIO_SCLK		15							// SPI Clock
#define GPIO_MOSI		14
#define GPIO_MISO		34 


extern const char *TAG;
extern scr_driver_t			lcd_drv;
extern touch_panel_driver_t		touch_drv;


// 13 bit PWM,  8191=Brightest, 0=off
void led_pwm_init()
{
	ledc_timer_config_t	ledc_timer = 
	{
		.speed_mode		= LEDC_LOW_SPEED_MODE,
		.timer_num		= LEDC_TIMER_0,
		.duty_resolution	= LEDC_TIMER_13_BIT,
		.freq_hz		= 5000,
		.clk_cfg		= LEDC_AUTO_CLK
	};
	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

	ledc_channel_config_t	ledc_channel =
	{
		.speed_mode		= LEDC_LOW_SPEED_MODE,
		.channel		= LEDC_CHANNEL_0,
		.timer_sel		= LEDC_TIMER_0,
		.intr_type		= LEDC_INTR_DISABLE,
		.gpio_num		= GPIO_LCDBL,
		.duty			= 0,
		.hpoint			= 0
	};
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}


// set LCD backlight brightness, 8 bit value
void led_pwm_set(int b)
{
	ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (b*32)+31));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}


// See https://github.com/espressif/esp-iot-solution/blob/master/examples/hmi/lvgl_thermostat/main/app_main.c
// https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/basic/bus.html


// Inil lcd display and touch screen
void lcd_init()
{
	spi_bus_handle_t		bus_handle;
	scr_interface_driver_t 		*iface_drv;
	
	led_pwm_init();
	led_pwm_set(255);						// turn backlight on

	spi_config_t		bus_conf =
	{
		.miso_io_num	= GPIO_MISO,
		.mosi_io_num	= GPIO_MOSI,
		.sclk_io_num	= GPIO_SCLK,
		//.max_transfer_sz= 32,
	};
	//bus_handle = spi_bus_create(SPI3_HOST, &bus_conf);		// or SPI2_HOST ?
	bus_handle = spi_bus_create(SPI2_HOST, &bus_conf);		// or SPI2_HOST ?
	
	scr_interface_spi_config_t spi_lcd_cfg = 
	{
		.spi_bus    = bus_handle,
		.pin_num_cs = GPIO_LCDCS,
		.pin_num_dc = GPIO_DATACMD,
		.clk_freq   = SPI_SPEED_LCD_HZ,	
		.swap_data  = true,
	};
	scr_interface_create(SCREEN_IFACE_SPI, &spi_lcd_cfg, &iface_drv);
    
	scr_controller_config_t lcd_cfg = 
	{
		.interface_drv		= iface_drv,
		.pin_num_rst 		= GPIO_LCDRESET,
		.pin_num_bckl		= -1,
		.rst_active_level	= 0,
		.bckl_active_level	= 1,
		.offset_hor		= 0,
		.offset_ver		= 0,
		// Display resolution is specified in portrait
		.width			= 240,
		.height			= 320,
		//.rotate			= SCR_DIR_TBLR,
		.rotate			= SCR_DIR_LRBT,
	};

	scr_find_driver(SCREEN_CONTROLLER_ILI9341, &lcd_drv);
	ESP_LOGI(TAG, "JA doing lcd_drv init");
	lcd_drv.init(&lcd_cfg);


	// Same SPI MISO MOSI CLK pins as LCD
	touch_panel_config_t touch_cfg = 
	{
        	.interface_spi = 
		{
            		//.spi_bus = spi2_bus,
            		.spi_bus = bus_handle,
            		.pin_num_cs = GPIO_TCS,
            		.clk_freq = SPI_SPEED_TOUCH_HZ,
        	},
        	.interface_type = TOUCH_PANEL_IFACE_SPI,
        	.pin_num_int	= GPIO_TIRQ,
        	.width		= 240,
        	.height		= 320,
        	//.direction	= TOUCH_DIR_TBLR,
        	.direction	= TOUCH_DIR_LRBT,
    	};
	touch_panel_find_driver(TOUCH_PANEL_CONTROLLER_XPT2046, &touch_drv);
	ESP_LOGI(TAG, "JA doing touch_drv init");

	touch_drv.init(&touch_cfg);
	touch_drv.calibration_run(&lcd_drv, false);		// true=force, false read from flash if possible

	//gpio_pullup_en(GPIO_DATACMD);
	//gpio_pullup_en(GPIO_SCLK);
	//gpio_pullup_en(GPIO_TCS);
	//gpio_pullup_en(GPIO_LCDCS);

	//gpio_intr_disable(35);
	gpio_intr_disable(36);					// common issue
	//gpio_intr_disable(39);

	led_pwm_set(255);					// Just set max for now
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}


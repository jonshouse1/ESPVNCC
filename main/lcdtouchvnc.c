/* 
	LCD with touch screen.
	VNC connection to a host
*/

#define VNC_SERVER_IPADDR 	"192.168.1.111"
#define VNC_SERVER_SCREEN_NUM	1

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
#include "driver/gpio.h"
#include "lwip/sockets.h"

#include "global.h"
#include "lcd_ts_init.h"
#include "lcd_textbuf.h"
#include "jag.h"
#include "lcd_vncc.h"


// shared around
scr_driver_t				lcd_drv; 
touch_panel_driver_t 			touch_drv;


// globals
const char *TAG = "lcdtouchvnc";
char	client_ip[16];
char	server_ip[16];
int	online		= FALSE;					// TRUE = connected with an IP address?
int	link_up		= FALSE;					// TRUE = network cable plugged in
int	connection_state= 0;						// non zero when client_ip, server_ip or online change state




/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	uint8_t mac_addr[6] = {0};
	/* we can get the ethernet driver handle from event data */
	esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

	switch (event_id) 
	{
		case ETHERNET_EVENT_CONNECTED:
			esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
			link_up=TRUE;
			ESP_LOGI(TAG, "Ethernet Link Up");
			ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
				mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
			lcd_textbuf_printstring("Ethernet Link Up\n");
		break;

		case ETHERNET_EVENT_DISCONNECTED:
			link_up=FALSE;
			ESP_LOGI(TAG, "Ethernet Link Down");
			lcd_textbuf_printstring("Ethernet Link Down\n");
		break;

		case ETHERNET_EVENT_START:
			ESP_LOGI(TAG, "Ethernet Started");
			lcd_textbuf_printstring("Ethernet Started\n");
		break;

		case ETHERNET_EVENT_STOP:
			ESP_LOGI(TAG, "Ethernet Stopped");
			lcd_textbuf_printstring("Ethernet Stopped\n");
		break;

		default:
			ESP_LOGE(TAG," eth_event_handler() %d",event_id);
		break;
	}
	connection_state=1;									// prompt GUI to redraw
}



/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data)
{
	ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
	const esp_netif_ip_info_t *ip_info = &event->ip_info;
	char st[64];
    
	ESP_LOGI(TAG, "JA1 Ethernet Got IP Address");
	ESP_LOGI(TAG, "~~~~~~~~~~~");
	ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
	ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
	ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
	ESP_LOGI(TAG, "~~~~~~~~~~~");
	sprintf(client_ip, IPSTR, IP2STR(&ip_info->ip));

	lcd_textbuf_printstring("DHCP Completed\n\0");
	sprintf(st, "IP: "IPSTR"\n", IP2STR(&ip_info->ip));
	lcd_textbuf_printstring(st);
	sprintf(st, "NM: "IPSTR"\n", IP2STR(&ip_info->netmask));
	lcd_textbuf_printstring(st);
	sprintf(st, "GW: "IPSTR"\n", IP2STR(&ip_info->gw));
	lcd_textbuf_printstring(st);
	online=TRUE;
	connection_state=1;
	vncc_connect(VNC_SERVER_IPADDR, VNC_SERVER_SCREEN_NUM);
}


static void lost_ip_event_handler()
{
	bzero(&client_ip,sizeof(client_ip));
	ESP_LOGI(TAG, "Lost ethernet IP address");
	lcd_textbuf_printstring("Lost IP address\n");
	online=FALSE;
	connection_state=1;									// something has changed, prompt GUI code
}




#define	PIN_PHY_POWER	12					
void app_main(void)
{
	bzero(&lcd_drv, sizeof(scr_driver_t));
	lcd_init((scr_driver_t*)&lcd_drv, (touch_panel_driver_t*)&touch_drv);			// initialise the SPI LCD driver
	jag_init((scr_driver_t*)&lcd_drv, 240, 320);						// initialise my graphics library
	lcd_textbuf_init(&lcd_drv, &Font12, 23, 26);						// initialise the text terminal
	lcd_textbuf_setcolors(COLOR_WHITE, COLOR_BLUE);
	lcd_textbuf_enable(TRUE, TRUE);								// text terminal active and clear display
	lcd_textbuf_printstring("POE LCD VNC V0.8 ...\n\n");

	// Initialize TCP/IP network interface (should be called only once in application)
	ESP_ERROR_CHECK(esp_netif_init());
	// Create default event loop that running in background
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    	esp_netif_t *eth_netif = esp_netif_new(&cfg);
    
	// Set default handlers to process TCP/IP stuffs
	ESP_ERROR_CHECK(esp_eth_set_default_handlers(eth_netif));
	// Register user defined event handers
	ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, &lost_ip_event_handler, NULL));
    
	eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
	eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
	phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
	phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
	gpio_pad_select_gpio(PIN_PHY_POWER);
	gpio_set_direction(PIN_PHY_POWER,GPIO_MODE_OUTPUT);
	gpio_set_level(PIN_PHY_POWER, 1);
	vTaskDelay(pdMS_TO_TICKS(10));										
#if CONFIG_EXAMPLE_USE_INTERNAL_ETHERNET
	mac_config.smi_mdc_gpio_num = CONFIG_EXAMPLE_ETH_MDC_GPIO;
	mac_config.smi_mdio_gpio_num = CONFIG_EXAMPLE_ETH_MDIO_GPIO;
	esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
#if CONFIG_EXAMPLE_ETH_PHY_IP101
	esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_RTL8201
	esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_LAN8720
	esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_DP83848
	esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#endif
#elif CONFIG_EXAMPLE_USE_DM9051
	gpio_install_isr_service(0);
	spi_device_handle_t spi_handle = NULL;
	spi_bus_config_t buscfg = {
		.miso_io_num = CONFIG_EXAMPLE_DM9051_MISO_GPIO,
		.mosi_io_num = CONFIG_EXAMPLE_DM9051_MOSI_GPIO,
		.sclk_io_num = CONFIG_EXAMPLE_DM9051_SCLK_GPIO,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
	};
	ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_EXAMPLE_DM9051_SPI_HOST, &buscfg, 1));
	spi_device_interface_config_t devcfg = {
		.command_bits = 1,
		.address_bits = 7,
		.mode = 0,
		.clock_speed_hz = CONFIG_EXAMPLE_DM9051_SPI_CLOCK_MHZ * 1000 * 1000,
		.spics_io_num = CONFIG_EXAMPLE_DM9051_CS_GPIO,
		.queue_size = 20
	};
    
	ESP_ERROR_CHECK(spi_bus_add_device(CONFIG_EXAMPLE_DM9051_SPI_HOST, &devcfg, &spi_handle));
	/* dm9051 ethernet driver is based on spi driver */
	eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(spi_handle);
	dm9051_config.int_gpio_num = CONFIG_EXAMPLE_DM9051_INT_GPIO;
	esp_eth_mac_t *mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
	esp_eth_phy_t *phy = esp_eth_phy_new_dm9051(&phy_config);
#endif
    
	esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
	esp_eth_handle_t eth_handle = NULL;
	ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
	/* attach Ethernet driver to TCP/IP stack */
	ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
	/* start Ethernet driver state machine */
	ESP_ERROR_CHECK(esp_eth_start(eth_handle));

	//xTaskCreate(lcd_task, "lcdtask", 80*1024, NULL, configMAX_PRIORITIES-1, NULL);		// highest priority
	xTaskCreate(yafdp_server_task, "yafdp_server", 16384, NULL, 0, NULL);			// 0 = lowest priority
}




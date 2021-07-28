// Generic discovery protocol for devices 
//
//	protocol version (pvar)		0.3
//	J.Andrews, 1 Jul 2021
//
// (O)  Optional, may be blank or zero field
// (UC) Uppser Case
// (MC) Mixed case
// (LC) Lower case
// (ZT) is zero terminated string


//#define YAFDP_ALL_REPLIES_BROADCAST						// uncomment to send all responses as UDP broadcast


#define YAFDP_DISCOVERY_PORT				8118
#define YAFDP_BCAST_ADDR				"255.255.255.255"
#define YAFDP_MAGIC 					"YAFDP"
#define YAFDP_MAGIC_SIZE				6
#define YAFDP_TYPE_DISCOVERY_REQUEST_DEVICES		'D'
#define YAFDP_TYPE_DISCOVERY_REQUEST_SERVICE		'?'
#define YAFDP_TYPE_DISCOVERY_REQUEST_SERVICES		'S'
#define YAFDP_TYPE_DISCOVERY_REQUEST_MUTE		'M'
#define YAFDP_TYPE_DISCOVERY_REPLY_DEVICE		'd'			// Replying with a yafdp_reply_device structure
#define YAFDP_TYPE_DISCOVERY_REPLY_SERVICE		's'			// Replying with a yafdp_reply_service structure

#define YAFDP_SORT_BY_IP				0
#define YAFDP_SORT_BY_MAC				1


// List of short names for serices I have written
#define YAFDP_SERVICE_PROTOCOL_NAME_JLC			"JLC"			// Jons lighting controller (master)
#define YAFDP_SERVICE_PROTOCOL_NAME_JLD			"JLD"			// Jons lighting device (fixture)
#define YAFDP_SERVICE_PROTOCOL_NAME_JLB			"JLB"			// Jons lighting bridge (universe to physical)

#define YAFDP_MAX_SERVICES				100
#define YAFDP_MAX_SERVICE_LIST				100
#define YAFDP_MAX_LIST_DEVICES				1000
#define YAFDP_MAX_LIST_SERVICES				10000



// Request a reply from a given host (IP address)
// First 4 feilds of each structure should be the same
struct __attribute__((packed)) yafdp_request_devices
{
	char 		magic[YAFDP_MAGIC_SIZE];
	char		pver[2];						// Protocol version uint8 twice
	char		ptype;
	uint16_t	request_handle;						// Simple mechanism to ignore duplicate requests
// New field added 0.2 version of protocol, servers must still work with 0.1 version protocol requests so this field is considered optional
	uint16_t	udp_reply_port;						// UDP port replies are sent on
};





// Reply contains one of these
struct __attribute__((packed)) yafdp_reply_device
{
	char 		magic[YAFDP_MAGIC_SIZE];
	char		pver[2];						// Protocol version uint8 twice
	char		ptype;
	uint16_t	request_handle;						// Simple mechanism to ignore duplicate requests

	uint16_t	number_of_services;					// Now always 0
	char 		device_manufacturer[17];				// Manufacturer of physical device (UC)
	char		device_modelname[33];					// (O,MC,ZT)
	char		device_description[33];					// How the user describes the device itself (O,MC,ZT)
	char		device_location[33];					// How the user describes the device location (O,MC,ZT)
};




// yafdp_similar to reply_device plus an extra fields to store the IP address and MAC of the station sending the reply
struct __attribute__((packed)) yafdp_reply_device_list
{
	uint16_t	request_handle;						// Simple mechanism to ignore duplicate requests
	uint16_t	number_of_services;
	char 		device_manufacturer[17];
	char		device_modelname[33];
	char		device_description[33];	
	char		device_location[33];
	char 		ipaddr[16];
	char		mac[20];						// MAC address string, must be >18
	uint16_t	sort_order;						// order of lines after a sort
};





// Prototypes
void yafdp_parse_and_reply(char *rbuffer, int bsize, char *ipaddr);
void send_yafdp_discovery_reply_device(char *ipaddr,  int vers, int req_handle, int port);
void yafdp_parse_and_reply(char *rbuffer, int bsize, char *ipaddr);
void yafdp_server_init(char *manufacturer, char *modelname, char *device_description, char *location);
int udp_generic_send(char *d, int len, char *destination_ip, int destination_port, int broadcast);



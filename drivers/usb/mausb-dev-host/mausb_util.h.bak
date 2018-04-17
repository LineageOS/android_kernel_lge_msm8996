#include "mausb.h"


#ifdef DEBUG
		
#define DBG_LEVEL_LOW	0
#define DBG_LEVEL_MEDIUM 1
#define DBG_LEVEL_HIGH	2
#define DBG_LEVEL_MAX	3

#define DATA_TRANS_RX	0
#define DATA_TRANS_TX 	1
#define DATA_TRANS_MGMT	2
#define DATA_TRANS_PAL	3
#define DATA_TRANS_IN	4
#define DATA_TRANS_OUT	5
#define DATA_TRANS_MAIN	6
#define DATA_TRANS_DUMP	7






#define CURRENTLEVEL	0//sandeep
		
#define LG_PRINT(dbglvl,trnsfr,fmt,args...) \
				if(dbglvl >= CURRENTLEVEL )	\
				{ \
					if (trnsfr == DATA_TRANS_RX)	\
						printk(KERN_INFO " \nMAUSB: RX:"fmt,##args); \
					if (trnsfr == DATA_TRANS_TX)	\
						printk(KERN_INFO " \nMAUSB: TX:"fmt,##args); \
					if (trnsfr == DATA_TRANS_MGMT)	\
						printk(KERN_INFO " \nMAUSB: MGMT:"fmt,##args); \
					if (trnsfr == DATA_TRANS_PAL)	\
						printk(KERN_INFO " \nMAUSB: PAL:"fmt,##args); \
					if (trnsfr == DATA_TRANS_IN)	\
						printk(KERN_INFO " \nMAUSB: IN:"fmt,##args); \
					if (trnsfr == DATA_TRANS_OUT)	\
						printk(KERN_INFO " \nMAUSB: OUT:"fmt,##args); \
					if (trnsfr == DATA_TRANS_MAIN)	\
						printk(KERN_INFO " \nMAUSB: MAIN:"fmt,##args); \
					if (trnsfr == DATA_TRANS_DUMP)	\
						printk(KERN_INFO " \nMAUSB: DUMP:"fmt,##args); \
				}	

#else

#define LG_PRINT(dbglvl,trnsfr,fmt,args...) \
	while(0);
			
#endif


int mausb_is_from_host(struct mausb_header *header);
int mausb_is_mgmt_pkt(struct mausb_header *header);
int mausb_is_data_pkt(struct mausb_header *header);
int mausb_is_in_data_pkt(struct mausb_header *header);
int mausb_is_out_data_pkt(struct mausb_header *header);
unsigned short int mausb_get_ep_number(struct mausb_header *header);
unsigned short int mausb_get_usb_device_address(struct mausb_header *header);
unsigned short int mausb_get_usb_bus_number(struct mausb_header *header);
unsigned char mausb_get_request_id(struct mausb_header *header);
unsigned char mausb_get_sequence_number(struct mausb_header *header);

unsigned char mausb_get_uint8(unsigned char *buff, const int offset, const int length);
unsigned short mausb_get_uint16(unsigned char *buff, const int offset, const int length);
unsigned int mausb_get_uint24(unsigned char *buff, const int offset, const int length);


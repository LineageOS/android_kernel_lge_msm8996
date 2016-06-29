/**===================================================================
 * Copyright(c) 2009 LG Electronics Inc. All Rights Reserved
 *
 * File Name : broadcast_fc8080.h
 * Description : EDIT HISTORY FOR MODULE
 * This section contains comments describing changes made to the module.
 * Notice that changes are listed in reverse chronological order.
 *
 * when            model        who            what
 * 10.27.2009        android        inb612        Create for Android platform
====================================================================**/
#ifndef _BROADCAST_FC8080_H_
#define _BROADCAST_FC8080_H_
#include "../../broadcast_tdmb_typedef.h"
#include "../../broadcast_tdmb_drv_ifdef.h"

extern int broadcast_fc8080_drv_if_power_on(void);
extern int broadcast_fc8080_drv_if_power_off(void);
extern int broadcast_fc8080_drv_if_init(void);
extern int broadcast_fc8080_drv_if_stop(void);
extern int broadcast_fc8080_drv_if_set_channel(unsigned int freq_num, unsigned int subch_id, unsigned int op_mode);
extern int broadcast_fc8080_drv_if_detect_sync(int op_mode);
extern int broadcast_fc8080_drv_if_get_sig_info(struct broadcast_tdmb_sig_info *dmb_bb_info);
extern int broadcast_fc8080_drv_if_get_fic(char* buffer, unsigned int* buffer_size);
extern int broadcast_fc8080_drv_if_get_msc(char** buffer_ptr, unsigned int* buffer_size, unsigned int user_buffer_size);
extern int broadcast_fc8080_drv_if_reset_ch(void);
extern int broadcast_fc8080_drv_if_user_stop(int mode);
extern int broadcast_fc8080_drv_if_select_antenna(unsigned int sel);
extern int broadcast_fc8080_drv_if_set_nation(unsigned int nation);
extern int broadcast_fc8080_drv_if_is_on(void);
extern int broadcast_fc8080_drv_if_isr(void);


int tdmb_fc8080_power_on(void);
int tdmb_fc8080_power_off(void);
int tdmb_fc8080_select_antenna(unsigned int sel);
int tdmb_fc8080_i2c_write_burst(uint16 waddr, uint8* wdata, int length);
int tdmb_fc8080_i2c_read_burst(uint16 raddr, uint8* rdata, int length);
int tdmb_fc8080_mdelay(int32 ms);
void tdmb_fc8080_Must_mdelay(int32 ms);
void tdmb_fc8080_interrupt_lock(void);
void tdmb_fc8080_interrupt_free(void);
int tdmb_fc8080_spi_write_read(uint8* tx_data, int tx_length, uint8 *rx_data, int rx_length);
void tdmb_fc8080_set_userstop(int mode);
int tdmb_fc8080_tdmb_is_on(void);
/*[BCAST002][S] 20140804 seongeun.jin - modify chip init check timing issue on BLT*/
#ifdef FEATURE_POWER_ON_RETRY
int tdmb_fc8080_power_on_retry(void);
#endif
/*[BCAST002][E]*/

#if defined(CONFIG_ARCH_MSM8994)
#define __broadcast_dev_exit_p(x)        x
#define __broadcast_dev_init            __init
#elif defined(CONFIG_ARCH_MSM8996)
#define __broadcast_dev_exit_p(x)        x
#define __broadcast_dev_init            __init
#elif defined(CONFIG_ARCH_MSM8226)
#define __broadcast_dev_exit_p(x)        __devexit_p(x)
#define __broadcast_dev_init            __devinit
#endif

#endif
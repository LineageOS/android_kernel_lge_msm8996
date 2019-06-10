
#ifndef __ANX7688_DP_H
#define __ANX7688_DP_H

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "anx7688_core.h"
#include "anx_i2c_intf.h"
#include "../../../video/msm/mdss/mdss_hdmi_slimport.h"
#include "anx7688_mi1.h"

/* RX INFO */
#define RX_BANDWIDTH 0X85
#define RX_LANECOUNT 0x86

/* DP STATUS */
#define NONE_STATE 0
#define DP_HPD_ASSERT 1
#define DP_HPD_IRQ_LINK_TRAINING 2
#define EDID_READY 3
#define HDMI_INPUT_VIDEO_STABLE 4
#define DP_VIDEO_STABLE 5
#define DP_AUDIO_STABLE 6
#define PLAY_BACK 7

/* DPCD */
#define POLLING_EN	 0X02
#define POLLING_ERR 0x10
#define AUX_ERR	1
#define AUX_OK	0
#define SP_TX_INT_STATUS1 0xF7
#define TX_DEBUG1 0xB0

#define AUX_CTRL 0xE5
#define AUX_ADDR_7_0 0xE6
#define AUX_ADDR_15_8 0xE7
#define AUX_ADDR_19_16 0xE8
#define AUX_CTRL2 0xE9
#define BUF_DATA_COUNT 0xE4
#define MAX_BUF_CNT 16
#define BUF_DATA_0 0xF0

#define AUX_OP_EN 0x01
#define RST_CTRL2 0x07
#define AUX_RST 0x04
#define SP_TX_AUX_STATUS 0xE0

/* to access global platform data */
unsigned int sp_get_rx_lanecnt(void);
unsigned char sp_get_rx_bw(void);
void dp_init_variables(struct anx7688_chip *chip);
void dp_read_hpd_status(struct anx7688_chip *chip);
int dp_read_display_status(void);
int init_anx7688_dp(struct device *dev, struct anx7688_chip *chip);
void dp_variables_remove(struct device *dev);
void dp_print_status(int ret);
int dp_create_sysfs_interface(struct device *dev);
void rx_set_cable_type(void);
int rx_get_cable_type(void);
bool is_vga_dongle(void);
void sp_rx_cur_info(void);
#endif /* __ANX7688_DP_H */

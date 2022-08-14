/*
 *  drivers/media/radio/si470x/radio-si470x.h
 *
 *  Driver for radios with Silicon Labs Si470x FM Radio Receivers
 *
 *  Copyright (c) 2009 Tobias Lorenz <tobias.lorenz@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


/* driver definitions */
#define DRIVER_NAME "radio-si470x"

/* kernel includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-device.h>
#include <asm/unaligned.h>

/**************************************************************************
 * Register Definitions
 **************************************************************************/
#define RADIO_REGISTER_SIZE	2	/* 16 register bit width */
#define RADIO_REGISTER_NUM	16	/* DEVICEID   ... RDSD */
#define RDS_REGISTER_NUM	6	/* STATUSRSSI ... RDSD */

#define DEVICEID		0	/* Device ID */
#define DEVICEID_PN		0xf000	/* bits 15..12: Part Number */
#define DEVICEID_MFGID		0x0fff	/* bits 11..00: Manufacturer ID */

#define CHIPID			1	/* Chip ID */
#define CHIPID_REV		0xfc00	/* bits 15..10: Chip Version */
#define CHIPID_DEV		0x0200	/* bits 09..09: Device */
#define CHIPID_FIRMWARE		0x01ff	/* bits 08..00: Firmware Version */

#define POWERCFG		2	/* Power Configuration */
#define POWERCFG_DSMUTE		0x8000	/* bits 15..15: Softmute Disable */
#define POWERCFG_DMUTE		0x4000	/* bits 14..14: Mute Disable */
#define POWERCFG_MONO		0x2000	/* bits 13..13: Mono Select */
#define POWERCFG_RDSM		0x0800	/* bits 11..11: RDS Mode (Si4701 only) */
#define POWERCFG_SKMODE		0x0400	/* bits 10..10: Seek Mode */
#define POWERCFG_SEEKUP		0x0200	/* bits 09..09: Seek Direction */
#define POWERCFG_SEEK		0x0100	/* bits 08..08: Seek */
#define POWERCFG_DISABLE	0x0040	/* bits 06..06: Powerup Disable */
#define POWERCFG_ENABLE		0x0001	/* bits 00..00: Powerup Enable */

#define CHANNEL			3	/* Channel */
#define CHANNEL_TUNE		0x8000	/* bits 15..15: Tune */
#define CHANNEL_CHAN		0x03ff	/* bits 09..00: Channel Select */

#define SYSCONFIG1		4	/* System Configuration 1 */
#define SYSCONFIG1_RDSIEN	0x8000	/* bits 15..15: RDS Interrupt Enable (Si4701 only) */
#define SYSCONFIG1_STCIEN	0x4000	/* bits 14..14: Seek/Tune Complete Interrupt Enable */
#define SYSCONFIG1_RDS		0x1000	/* bits 12..12: RDS Enable (Si4701 only) */
#define SYSCONFIG1_DE		0x0800	/* bits 11..11: De-emphasis (0=75us 1=50us) */
#define SYSCONFIG1_AGCD		0x0400	/* bits 10..10: AGC Disable */
#define SYSCONFIG1_BLNDADJ	0x00c0	/* bits 07..06: Stereo/Mono Blend Level Adjustment */
#define SYSCONFIG1_GPIO3	0x0030	/* bits 05..04: General Purpose I/O 3 */
#define SYSCONFIG1_GPIO2	0x000c	/* bits 03..02: General Purpose I/O 2 */
#define SYSCONFIG1_GPIO1	0x0003	/* bits 01..00: General Purpose I/O 1 */

#define SYSCONFIG2		5	/* System Configuration 2 */
#define SYSCONFIG2_SEEKTH	0xff00	/* bits 15..08: RSSI Seek Threshold */
#define SYSCONFIG2_BAND		0x00c0	/* bits 07..06: Band Select */
#define SYSCONFIG2_SPACE	0x0030	/* bits 05..04: Channel Spacing */
#define SYSCONFIG2_VOLUME	0x000f	/* bits 03..00: Volume */

#define SYSCONFIG3		6	/* System Configuration 3 */
#define SYSCONFIG3_SMUTER	0xc000	/* bits 15..14: Softmute Attack/Recover Rate */
#define SYSCONFIG3_SMUTEA	0x3000	/* bits 13..12: Softmute Attenuation */
#define SYSCONFIG3_SKSNR	0x00f0	/* bits 07..04: Seek SNR Threshold */
#define SYSCONFIG3_SKCNT	0x000f	/* bits 03..00: Seek FM Impulse Detection Threshold */

#define TEST1			7	/* Test 1 */
#define TEST1_AHIZEN		0x4000	/* bits 14..14: Audio High-Z Enable */

#define TEST2			8	/* Test 2 */
/* TEST2 only contains reserved bits */

#define BOOTCONFIG		9	/* Boot Configuration */
/* BOOTCONFIG only contains reserved bits */

#define STATUSRSSI		10	/* Status RSSI */
#define STATUSRSSI_RDSR		0x8000	/* bits 15..15: RDS Ready (Si4701 only) */
#define STATUSRSSI_STC		0x4000	/* bits 14..14: Seek/Tune Complete */
#define STATUSRSSI_SF		0x2000	/* bits 13..13: Seek Fail/Band Limit */
#define STATUSRSSI_AFCRL	0x1000	/* bits 12..12: AFC Rail */
#define STATUSRSSI_RDSS		0x0800	/* bits 11..11: RDS Synchronized (Si4701 only) */
#define STATUSRSSI_BLERA	0x0600	/* bits 10..09: RDS Block A Errors (Si4701 only) */
#define STATUSRSSI_ST		0x0100	/* bits 08..08: Stereo Indicator */
#define STATUSRSSI_RSSI		0x00ff	/* bits 07..00: RSSI (Received Signal Strength Indicator) */

#define READCHAN		11	/* Read Channel */
#define READCHAN_BLERB		0xc000	/* bits 15..14: RDS Block D Errors (Si4701 only) */
#define READCHAN_BLERC		0x3000	/* bits 13..12: RDS Block C Errors (Si4701 only) */
#define READCHAN_BLERD		0x0c00	/* bits 11..10: RDS Block B Errors (Si4701 only) */
#define READCHAN_READCHAN	0x03ff	/* bits 09..00: Read Channel */

#define RDSA			12	/* RDSA */
#define RDSA_RDSA		0xffff	/* bits 15..00: RDS Block A Data (Si4701 only) */

#define RDSB			13	/* RDSB */
#define RDSB_RDSB		0xffff	/* bits 15..00: RDS Block B Data (Si4701 only) */

#define RDSC			14	/* RDSC */
#define RDSC_RDSC		0xffff	/* bits 15..00: RDS Block C Data (Si4701 only) */

#define RDSD			15	/* RDSD */
#define RDSD_RDSD		0xffff	/* bits 15..00: RDS Block D Data (Si4701 only) */

#define TURN_ON 1
#define TURN_OFF 0
#define SI470X_DELAY_MSEC 10

#define CH_SPACING_200 200
#define CH_SPACING_100 100
#define CH_SPACING_50 50


#define SRCH_UP          1
#define SRCH_DOWN       `0

#define WRAP_ENABLE      1
#define WRAP_DISABLE     0

/* Standard buffer size */
#define STD_BUF_SIZE     256

/* to distinguish between seek, tune during STC int. */
#define NO_SEEK_TUNE_PENDING 0
#define TUNE_PENDING 1
#define SEEK_PENDING 2
#define SCAN_PENDING 3

#define SI470X_MIN_SRCH_MODE 0x00
#define SI470X_MAX_SRCH_MODE 0x03

#define MIN_DWELL_TIME 0x00
#define MAX_DWELL_TIME 0x0F

#define TUNE_STEP_SIZE 10
#define NO_OF_RDS_BLKS 4

#define GET_MSB(x)((x >> 8) & 0xFF)
#define GET_LSB(x)((x) & 0xFF)

#define OFFSET_OF_GRP_TYP 11
#define RDS_INT_BIT 0x01
#define FIFO_CNT_16 0x10
#define UNCORRECTABLE_RDS_EN 0xFF01

#define APP_GRP_typ_MASK	0x1F

#define MSB_OF_BLK_0 4
#define LSB_OF_BLK_0 5
#define MSB_OF_BLK_1 6
#define LSB_OF_BLK_1 7
#define MSB_OF_BLK_2 8
#define LSB_OF_BLK_2 9
#define MSB_OF_BLK_3 10
#define LSB_OF_BLK_3 11
#define MAX_RT_LEN 64
#define END_OF_RT 0x0d
#define MAX_PS_LEN 8
#define OFFSET_OF_PS 5
#define PS_VALIDATE_LIMIT 2
#define RT_VALIDATE_LIMIT 2
#define RDS_CMD_LEN 3
#define RDS_RSP_LEN 13
#define PS_EVT_DATA_LEN (MAX_PS_LEN + OFFSET_OF_PS)
#define NO_OF_PS 1
#define OFFSET_OF_RT 5
#define OFFSET_OF_PTY 5
#define MAX_LEN_2B_GRP_RT 32
#define CNT_FOR_2A_GRP_RT 4
#define CNT_FOR_2B_GRP_RT 2
#define PS_MASK 0x3
#define PTY_MASK 0x1F
#define NO_OF_CHARS_IN_EACH_ADD 2

#define RDS_TYPE_0A     (0 * 2 + 0)
#define RDS_TYPE_0B     (0 * 2 + 1)
#define RDS_TYPE_2A     (2 * 2 + 0)
#define RDS_TYPE_2B     (2 * 2 + 1)
#define RDS_TYPE_3A     (3 * 2 + 0)
#define UNCORRECTABLE           3

#define APP_GRP_typ_MASK	0x1F
/*ERT*/
#define ERT_AID			0x6552
#define MAX_ERT_SEGMENT		31
#define MAX_ERT_LEN		256
#define ERT_OFFSET		3
#define ERT_FORMAT_DIR_BIT	1
#define ERT_CNT_PER_BLK		2
/*RT PLUS*/
#define DUMMY_CLASS		0
#define RT_PLUS_LEN_1_TAG	3
#define RT_ERT_FLAG_BIT		13
#define RT_PLUS_AID             0x4bd7
#define RT_ERT_FLAG_OFFSET	1
#define RT_PLUS_OFFSET		2
/*TAG1*/
#define TAG1_MSB_OFFSET		3
#define TAG1_MSB_MASK		7
#define TAG1_LSB_OFFSET		13
#define TAG1_POS_MSB_MASK	0x3F
#define TAG1_POS_MSB_OFFSET	1
#define TAG1_POS_LSB_OFFSET	7
#define TAG1_LEN_OFFSET		1
#define TAG1_LEN_MASK		0x3F
/*TAG2*/
#define TAG2_MSB_OFFSET		5
#define TAG2_MSB_MASK		9
#define TAG2_LSB_OFFSET		11
#define TAG2_POS_MSB_MASK	0x3F
#define TAG2_POS_MSB_OFFSET	3
#define TAG2_POS_LSB_OFFSET	5
#define TAG2_LEN_MASK		0x1F

#define DEFAULT_AF_RSSI_LOW_TH 25
#define NO_OF_AF_IN_GRP 2
#define MAX_NO_OF_AF 25
#define MAX_AF_LIST_SIZE (MAX_NO_OF_AF * 4) /* 4 bytes per freq */
#define GET_AF_EVT_LEN(x) (7 + x*4)
#define GET_AF_LIST_LEN(x) (x*4)
#define MIN_AF_FREQ_CODE 1
#define MAX_AF_FREQ_CODE 204
/* 25 AFs supported for a freq. 224 means 1 AF. 225 means 2 AFs and so on */
#define NO_AF_CNT_CODE 224
#define MIN_AF_CNT_CODE 225
#define MAX_AF_CNT_CODE 249
#define AF_WAIT_SEC 10
#define MAX_AF_WAIT_SEC 255
#define AF_PI_WAIT_TIME 50 /* 50*100msec = 5sec */
/* freqs are divided by 10. */
#define SCALE_AF_CODE_TO_FREQ_KHZ(x) (87500 + (x*100))

#define EXTRACT_BIT(data, bit_pos) ((data >> bit_pos) & 1)

/* Write starts with the upper byte of register 0x02 */
#define WRITE_REG_NUM		8
#define WRITE_INDEX(i)		(i + 0x02)

/* Read starts with the upper byte of register 0x0a */
#define READ_REG_NUM		RADIO_REGISTER_NUM
#define READ_INDEX(i)		((i + RADIO_REGISTER_NUM - 0x0a) % READ_REG_NUM)

#define CORRECTED_NONE          0
#define CORRECTED_ONE_TO_TWO    1
#define CORRECTED_THREE_TO_FIVE 2
#define UNCORRECTABLE           3
#define ERRORS_CORRECTED(data,block) ((data>>block)&0x03)
/*Block Errors are reported in .5% increments*/
#define BLER_SCALE_MAX 200

/*
 * si470x_device - private data
 */

enum si470x_buf_t {
	SILABS_FM_BUF_SRCH_LIST,
	SILABS_FM_BUF_EVENTS,
	SILABS_FM_BUF_RT_RDS,
	SILABS_FM_BUF_PS_RDS,
	SILABS_FM_BUF_RAW_RDS,
	SILABS_FM_BUF_AF_LIST,
	SILABS_FM_BUF_RT_PLUS = 11,
	SILABS_FM_BUF_ERT,
	SILABS_FM_BUF_MAX
};

enum si470x_evt_t {
	SILABS_EVT_RADIO_READY,
	SILABS_EVT_TUNE_SUCC,
	SILABS_EVT_SEEK_COMPLETE,
	SILABS_EVT_SCAN_NEXT,
	SILABS_EVT_NEW_RAW_RDS,
	SILABS_EVT_NEW_RT_RDS,
	SILABS_EVT_NEW_PS_RDS,
	SILABS_EVT_ERROR,
	SILABS_EVT_BELOW_TH,
	SILABS_EVT_ABOVE_TH,
	SILABS_EVT_STEREO,
	SILABS_EVT_MONO,
	SILABS_EVT_RDS_AVAIL,
	SILABS_EVT_RDS_NOT_AVAIL,
	SILABS_EVT_NEW_SRCH_LIST,
	SILABS_EVT_NEW_AF_LIST,
	SILABS_EVT_TXRDSDAT,
	SILABS_EVT_TXRDSDONE,
	SILABS_EVT_RADIO_DISABLED,
	SILABS_EVT_NEW_ODA,
	SILABS_EVT_NEW_RT_PLUS,
	SILABS_EVT_NEW_ERT
};

struct si470x_recv_conf_req {
	__u16	emphasis;
	__u16	ch_spacing;
	/* limits stored as actual freq / TUNE_STEP_SIZE */
	__u16	band_low_limit;
	__u16	band_high_limit;
};

struct si470x_rel_freq {
	__u8  rel_freq_msb;
	__u8  rel_freq_lsb;
} __packed;

struct si470x_srch_list_compl {
	__u8    num_stations_found;
	struct si470x_rel_freq  rel_freq[20];
} __packed;

struct af_list_ev {
	__le32   tune_freq_khz;
	__le16   pi_code;
	__u8    af_size;
	__u8    af_list[MAX_AF_LIST_SIZE];
} __packed;

struct si470x_af_info {
	/* no. of invalid AFs. */
	u8 inval_freq_cnt;
	/* no. of AFs in the list. */
	u8 cnt;
	/* actual size of the list */
	u8 size;
	/* index of currently tuned station in the AF list. */
	u8 index;
	/* PI of the frequency */
	u16 pi;
	/* freq to which AF list belongs to. */
	u32 orig_freq_khz;
	/* AF list */
	u32 af_list[MAX_NO_OF_AF];
};

struct si470x_device {
	struct pwm_device *pwm;
	bool is_len_gpio_valid;
	struct si470x_vreg_data *dreg;
	struct si470x_vreg_data *areg;
	int reset_gpio;
	int int_gpio;
	int status_gpio;
	struct pinctrl *fm_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
	struct v4l2_device v4l2_dev;
	struct video_device *videodev;
	struct v4l2_ctrl_handler hdl;
	int band;
	int space;
	unsigned int mode;
	u8 seek_tune_status;
	/* Silabs internal registers (0..15) */
	unsigned short registers[RADIO_REGISTER_NUM];

	/* RDS receive buffer */
	wait_queue_head_t read_queue;
	int irq;
	int tuned_freq_khz;
	int dwell_time_sec;
	struct mutex lock;		/* buffer locking */
	unsigned char *buffer;		/* size is always multiple of three */
	u8 write_buf[WRITE_REG_NUM];
	/* TO read events, data*/
	u8 read_buf[READ_REG_NUM];

	u16 pi; /* PI of tuned channel */
	u8 pty; /* programe type of the tuned channel */

	u16 block[NO_OF_RDS_BLKS];
	u8 rt_display[MAX_RT_LEN];   /* RT that will be displayed */
	u8 rt_tmp0[MAX_RT_LEN]; /* high probability RT */
	u8 rt_tmp1[MAX_RT_LEN]; /* low probability RT */
	u8 rt_cnt[MAX_RT_LEN];  /* high probability RT's hit count */
	u8 rt_flag;          /* A/B flag of RT */
	bool valid_rt_flg;     /* validity of A/B flag */
	u8 ps_display[MAX_PS_LEN];    /* PS that will be displayed */
	u8 ps_tmp0[MAX_PS_LEN]; /* high probability PS */
	u8 ps_tmp1[MAX_PS_LEN]; /* low probability PS */
	u8 ps_cnt[MAX_PS_LEN];  /* high probability PS's hit count */
	u8 bler[NO_OF_RDS_BLKS];
	u8 rt_plus_carrier;
	u8 ert_carrier;
	u8 ert_buf[MAX_ERT_LEN];
	u8 ert_len;
	u8 c_byt_pair_index;
	u8 utf_8_flag;
	u8 rt_ert_flag;
	u8 formatting_dir;

	unsigned int buf_size;
	unsigned int rd_index;
	unsigned int wr_index;

	/* buffer locks*/
	spinlock_t buf_lock[SILABS_FM_BUF_MAX];

	/*RDS buffers + Radio event buffer*/
	struct kfifo data_buf[SILABS_FM_BUF_MAX];
	struct si470x_recv_conf_req recv_conf;
	struct completion completion;
	bool status_rssi_auto_update;	/* Does RSSI get updated automatic? */
	bool is_search_cancelled;
	u8 g_search_mode;

	struct si470x_srch_list_compl srch_list;
	struct si470x_af_info af_info1;
	struct si470x_af_info af_info2;

	struct workqueue_struct *wqueue;
	struct workqueue_struct *wqueue_scan;
	struct workqueue_struct *wqueue_rds;
	struct work_struct rds_worker;

	struct delayed_work work;
	struct delayed_work work_scan;
	/* wait queue for blocking event read */
	wait_queue_head_t event_queue;

#if IS_ENABLED(CONFIG_USB_SI470X)
	/* reference to USB and video device */
	struct usb_device *usbdev;
	struct usb_interface *intf;
	char *usb_buf;

	/* Interrupt endpoint handling */
	char *int_in_buffer;
	struct usb_endpoint_descriptor *int_in_endpoint;
	struct urb *int_in_urb;
	int int_in_running;

	/* scratch page */
	unsigned char software_version;
	unsigned char hardware_version;
#endif

#ifdef CONFIG_I2C_SI470X
	struct i2c_client *client;
#endif
};

enum radio_state_t {
	FM_OFF,
	FM_RECV,
	FM_RESET,
	FM_CALIB,
	FM_TURNING_OFF,
	FM_RECV_TURNING_ON,
	FM_MAX_NO_STATES,
};

struct si470x_vreg_data {
	/* voltage regulator handle */
	struct regulator *reg;
	/* regulator name */
	const char *name;
	/* voltage levels to be set */
	unsigned int low_vol_level;
	unsigned int high_vol_level;
	bool set_voltage_sup;
	/* is this regulator enabled? */
	bool is_enabled;
};

enum search_t {
	SEEK,
	SCAN,
	SCAN_FOR_STRONG,
	SEEK_STOP,
};

/**************************************************************************
 * Firmware Versions
 **************************************************************************/

#define RADIO_FW_VERSION	12

/**************************************************************************
 * Frequency Multiplicator
 **************************************************************************/

/*
 * The frequency is set in units of 62.5 Hz when using V4L2_TUNER_CAP_LOW,
 * 62.5 kHz otherwise.
 * The tuner is able to have a channel spacing of 50, 100 or 200 kHz.
 * tuner->capability is therefore set to V4L2_TUNER_CAP_LOW
 * The FREQ_MUL is then: 1 MHz / 62.5 Hz = 16000
 */
#define FREQ_MUL (1000000 / 62.5)

enum v4l2_cid_private_silabs_fm_t {
	V4L2_CID_PRIVATE_SILABS_SRCHMODE = (V4L2_CID_PRIVATE_BASE + 1),
	V4L2_CID_PRIVATE_SILABS_SCANDWELL,
	V4L2_CID_PRIVATE_SILABS_SRCHON,
	V4L2_CID_PRIVATE_SILABS_STATE,
	V4L2_CID_PRIVATE_SILABS_TRANSMIT_MODE,
	V4L2_CID_PRIVATE_SILABS_RDSGROUP_MASK,
	V4L2_CID_PRIVATE_SILABS_REGION,
	V4L2_CID_PRIVATE_SILABS_SIGNAL_TH,
	V4L2_CID_PRIVATE_SILABS_SRCH_PTY,
	V4L2_CID_PRIVATE_SILABS_SRCH_PI,
	V4L2_CID_PRIVATE_SILABS_SRCH_CNT,
	V4L2_CID_PRIVATE_SILABS_EMPHASIS,
	V4L2_CID_PRIVATE_SILABS_RDS_STD,
	V4L2_CID_PRIVATE_SILABS_SPACING,
	V4L2_CID_PRIVATE_SILABS_RDSON,
	V4L2_CID_PRIVATE_SILABS_RDSGROUP_PROC,
	V4L2_CID_PRIVATE_SILABS_LP_MODE,
	V4L2_CID_PRIVATE_SILABS_ANTENNA,
	V4L2_CID_PRIVATE_SILABS_RDSD_BUF,
	V4L2_CID_PRIVATE_SILABS_PSALL,
	/*v4l2 Tx controls*/
	V4L2_CID_PRIVATE_SILABS_TX_SETPSREPEATCOUNT,
	V4L2_CID_PRIVATE_SILABS_STOP_RDS_TX_PS_NAME,
	V4L2_CID_PRIVATE_SILABS_STOP_RDS_TX_RT,
	V4L2_CID_PRIVATE_SILABS_IOVERC,
	V4L2_CID_PRIVATE_SILABS_INTDET,
	V4L2_CID_PRIVATE_SILABS_MPX_DCC,
	V4L2_CID_PRIVATE_SILABS_AF_JUMP,
	V4L2_CID_PRIVATE_SILABS_RSSI_DELTA,
	V4L2_CID_PRIVATE_SILABS_HLSI,

	/*
	* Here we have IOCTl's that are specific to IRIS
	* (V4L2_CID_PRIVATE_BASE + 0x1E to V4L2_CID_PRIVATE_BASE + 0x28)
	*/
	V4L2_CID_PRIVATE_SILABS_SOFT_MUTE,/* 0x800001E*/
	V4L2_CID_PRIVATE_SILABS_RIVA_ACCS_ADDR,
	V4L2_CID_PRIVATE_SILABS_RIVA_ACCS_LEN,
	V4L2_CID_PRIVATE_SILABS_RIVA_PEEK,
	V4L2_CID_PRIVATE_SILABS_RIVA_POKE,
	V4L2_CID_PRIVATE_SILABS_SSBI_ACCS_ADDR,
	V4L2_CID_PRIVATE_SILABS_SSBI_PEEK,
	V4L2_CID_PRIVATE_SILABS_SSBI_POKE,
	V4L2_CID_PRIVATE_SILABS_TX_TONE,
	V4L2_CID_PRIVATE_SILABS_RDS_GRP_COUNTERS,
	V4L2_CID_PRIVATE_SILABS_SET_NOTCH_FILTER,/* 0x8000028 */

	V4L2_CID_PRIVATE_SILABS_SET_AUDIO_PATH,/* 0x8000029 */
	V4L2_CID_PRIVATE_SILABS_DO_CALIBRATION,/* 0x800002A : IRIS */
	V4L2_CID_PRIVATE_SILABS_SRCH_ALGORITHM,/* 0x800002B */
	V4L2_CID_PRIVATE_SILABS_GET_SINR, /* 0x800002C : IRIS */
	V4L2_CID_PRIVATE_SILABS_INTF_LOW_THRESHOLD, /* 0x800002D */
	V4L2_CID_PRIVATE_SILABS_INTF_HIGH_THRESHOLD, /* 0x800002E */
	V4L2_CID_PRIVATE_SILABS_SINR_THRESHOLD,  /* 0x800002F : IRIS */
	V4L2_CID_PRIVATE_SILABS_SINR_SAMPLES,  /* 0x8000030 : IRIS */
	V4L2_CID_PRIVATE_SILABS_SPUR_FREQ,
	V4L2_CID_PRIVATE_SILABS_SPUR_FREQ_RMSSI,
	V4L2_CID_PRIVATE_SILABS_SPUR_SELECTION,
	V4L2_CID_PRIVATE_SILABS_UPDATE_SPUR_TABLE,
	V4L2_CID_PRIVATE_SILABS_VALID_CHANNEL,
	V4L2_CID_PRIVATE_SILABS_AF_RMSSI_TH,
	V4L2_CID_PRIVATE_SILABS_AF_RMSSI_SAMPLES,
	V4L2_CID_PRIVATE_SILABS_GOOD_CH_RMSSI_TH,
	V4L2_CID_PRIVATE_SILABS_SRCHALGOTYPE,
	V4L2_CID_PRIVATE_SILABS_CF0TH12,
	V4L2_CID_PRIVATE_SILABS_SINRFIRSTSTAGE,
	V4L2_CID_PRIVATE_SILABS_RMSSIFIRSTSTAGE,
	V4L2_CID_PRIVATE_SILABS_RXREPEATCOUNT,
	V4L2_CID_PRIVATE_SILABS_RSSI_TH, /* 0x800003E */
	V4L2_CID_PRIVATE_SILABS_AF_JUMP_RSSI_TH /* 0x800003F */

};

/**************************************************************************
 * Common Functions
 **************************************************************************/
extern struct video_device si470x_viddev_template;
int si470x_get_register(struct si470x_device *radio, int regnr);
int si470x_set_register(struct si470x_device *radio, int regnr);
int si470x_disconnect_check(struct si470x_device *radio);
int si470x_set_freq(struct si470x_device *radio, unsigned int freq);
int si470x_start(struct si470x_device *radio);
int si470x_stop(struct si470x_device *radio);
int si470x_set_chan(struct si470x_device *radio, unsigned short chan);
int si470x_fops_open(struct file *file);
int si470x_fops_release(struct file *file);
int si470x_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability);
void si470x_q_event(struct si470x_device *radio,
		enum si470x_evt_t event);
void si470x_scan(struct work_struct *work);
void si470x_search(struct si470x_device *radio, bool on);
int cancel_seek(struct si470x_device *radio);
void si470x_rds_handler(struct work_struct *worker);
int si470x_get_all_registers(struct si470x_device *radio);


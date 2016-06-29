#ifndef __ANX7418_H__
#define __ANX7418_H__

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#ifdef CONFIG_LGE_USB_TYPE_C
#include <linux/power_supply.h>
#endif
#include <linux/wakelock.h>
#include <linux/rwsem.h>
#include <linux/usb/class-dual-role.h>

#ifdef CONFIG_LGE_ALICE_FRIENDS
#include <soc/qcom/lge/board_lge.h>
#include "../alice_friends/hm.h"
#endif
#ifdef CONFIG_LGE_PM_CABLE_DETECTION
#include <soc/qcom/lge/lge_cable_detection.h>
#endif

#include "anx7418_i2c.h"
#include "anx7418_debug.h"
#include "anx7418_charger.h"
#include "anx7418_firmware.h"

#define IS_INTF_IRQ_SUPPORT(anx) \
	(anx->otp && (anx->rom_ver >= 0x11 && anx->rom_ver < 0xB1 && anx->rom_ver != 0x16))

struct anx7418 {
	struct i2c_client  *client;

	/* regulator */
	struct regulator *avdd33;
#ifdef CONFIG_LGE_USB_TYPE_C
	struct regulator *vbus_reg;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
#endif

	/* gpio */
	int pwr_en_gpio;
	int resetn_gpio;
	int vconn_gpio;
	int sbu_sel_gpio;
	int i2c_irq_gpio;
	int cable_det_gpio;
	int cable_det_irq;
#ifdef CONFIG_LGE_ALICE_FRIENDS
	int ext_acc_en_gpio;
	int ext_acc_en_irq;
#endif

	struct anx7418_charger chg;

	atomic_t pwr_on;

	struct workqueue_struct *wq;
	struct work_struct cable_det_work;
	struct work_struct i2c_work;

	struct rw_semaphore rwsem;
	struct wake_lock wlock;

	bool otp;
	int rom_ver;
	bool pd_restart;

	int mode;
	int pr;
	int dr;

	bool is_dbg_acc;

	struct work_struct try_src_work;
	struct work_struct try_snk_work;
	bool is_tried_snk;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;
#endif

#ifdef CONFIG_LGE_ALICE_FRIENDS
	enum lge_alice_friends friends;
	struct mutex hm_mutex;
	struct hm_instance *hm;
#endif
};

/* ANX7418 I2C registers */
#define VENDOR_ID_L			0x00
#define VENDOR_ID_H			0x01
#define DEVICE_ID_L			0x02
#define DEVICE_ID_H			0x03
#define DEVICE_VERSION			0x04

#define RESET_CTRL_0			0x05
#define R_HARDWARE_RESET		BIT(0)
#define R_SOFTWARE_RESET		BIT(1)
#define R_CONFIG_X_RESET		BIT(2)
#define R_PD_RESET			BIT(3)
#define R_OCM_RESET			BIT(4)

#define CONFIG_X_CTRL_0			0x06
#define CONFIG_X_CTRL_1			0x07
#define CONFIG_X_CTRL_2			0x08
#define CONFIG_X_CTRL_3			0x09
#define CONFIG_X_CTRL_4			0x0A
#define CONFIG_X_CTRL_5			0x0B
#define CONFIG_X_CTRL_6			0x0C

#define POWER_DOWN_CTRL			0x0D
#define R_POWER_DOWN_PD			BIT(0)
#define R_POWER_DOWN_OCM		BIT(1)
#define CC2_VRD_3P0			BIT(2)
#define CC2_VRD_1P5			BIT(3)
#define CC2_VRD_USB			BIT(4)
#define CC1_VRD_3P0			BIT(5)
#define CC1_VRD_1P5			BIT(6)
#define CC1_VRD_USB			BIT(7)

#define I2C_SLAVE_CTRL			0x0E
#define OCM_CTRO_0			0x0F
#define OCM_CTRO_1			0x10

#define TX_PTR_FRONT			0x11
#define TX_PTR_REAR			0x12
#define RX_PTR_FRONT			0x13
#define RX_PTR_REAR			0x14

#define RX_STATUS			0x15
#define TX_STATUS			0x16
#define STATUS_SUCCESS			BIT(0)
#define STATUS_ERROR			BIT(1)
#define OCM_STARTUP			BIT(7)

#define IRQ_INTF_MASK			0x17
#define RECVD_MSG			BIT(0)
#define VCONN_CHG			BIT(2)
#define VBUS_CHG			BIT(3)
#define CC_STATUS_CHG			BIT(4)
#define DATA_ROLE_CHG			BIT(5)
#define PR_C_GOT_POWER			BIT(6)

#define TX_DATA0			0x18 /* ~ 0x1F */
#define RX_DATA0			0x20 /* ~ 0x27 */

#define IRQ_INTF_STATUS			0x28
#define INTF_STATUS			0x29
#define VCONN_STATUS			BIT(2)
#define VBUS_STATUS			BIT(3)
#define DATA_ROLE			BIT(5)

#define CC_STATUS			0x2A
#define CC1_SRC_OPEN			0x00
#define CC1_SRC_RD			0x01
#define CC1_SRC_RA			0x02
#define CC1_SNK_DEFAULT			0x04
#define CC1_SNK_POWER15			0x08
#define CC1_SNK_POWER30			0x0C
#define CC2_SRC_OPEN			0x00
#define CC2_SRC_RD			0x10
#define CC2_SRC_RA			0x20
#define CC2_SNK_DEFAULT			0x40
#define CC2_SNK_POWER15			0x80
#define CC2_SNK_POWER30			0xC0

#define OCM_WDTL_H			0x2B
#define OCM_WDTL_M			0x2C
#define OCM_WDTL_L			0x2D
#define OCM_WDTH			0x2E
#define OCM_RD_ADDR			0x2F
#define OCM_RD_DATA			0x30
#define CONFIG_X_INTP			0x31
#define CONFIG_X_ACCESS_FIFO		0x32
#define INTR_CTRL			0x33
#define HPD_DEGLITCH_7_0		0x34
#define HPD_DEGLITCH_13_8		0x35

#define HPD_CTRL_0			0x36
#define R_HPD_OUT_DATA			BIT(4)
#define R_HPD_OEN			BIT(5)

#define OCM_INTER_POINTER_7_0		0x37
#define OCM_INTER_POINTER_15_8		0x38
#define IRQ_EXT_MASK_0			0x3B
#define IRQ_EXT_MASK_1			0x3C
#define IRQ_EXT_MASK_2			0x3D
#define IRQ_EXT_SOURCE_0		0x3E
#define GPIO_4_5			0x3F

#define ANALOG_STATUS			0x40
#define R_TRY_DFP			BIT(1)
#define DFP_OR_UFP			BIT(3)
#define UFP_PLUG			BIT(4)
#define PIN_VCONN_1_IN			BIT(5)
#define PIN_VCONN_2_IN			BIT(6)

#define ANALOG_CTRL_0			0x41
#define R_DRP_PERIOD			0x0F
#define R_DRP_PERCENT			0x70
#define R_PIN_CABLE_DET			BIT(7)

#define ANALOG_CTRL_1			0x42
#define R_SWITCH			0xFF

#define ANALOG_CTRL_2			0x43
#define R_MODE_TRANSITION		BIT(0)
#define R_CC_CAP_CC2			BIT(1)
#define R_CC_CAP_CC1			BIT(2)
#define R_LATCH_REG			BIT(3)
#define R_AUX_SWAP_0			BIT(4)
#define R_AUX_SWAP_1			BIT(5)
#define R_AUX_SWAP_2			BIT(6)
#define R_AUX_SWAP_3			BIT(7)

#define ANALOG_CTRL_3			0x44 // Firmware Version
#define ANALOG_CTRL_4			0x45

#define ANALOG_CTRL_5			0x46
#define R_SWITCH_H			0xF0

#define ANALOG_CTRL_6			0x47
#define R_RP				0x03
#define DRP_EN				BIT(2)

#define ANALOG_CTRL_7			0x48
#define CC2_5P1K			BIT(0)
#define CC2_RA				BIT(1)
#define CC1_5P1K			BIT(2)
#define CC1_RA				BIT(3)
#define R_WRITE_DELAY_COUNTER		0xF0

#define ANALOG_CTRL_8			0x49

#define ANALOG_CTRL_9			0x4A
#define CC_SOFT_EN			BIT(0)
#define BMC_ON_CC1			BIT(1)
#define BMC_TX_MODE			BIT(2)

#define ANALOG_CTRL_10			0x4B
#define ANALOG_CTRL_11			0x4C
#define ANALOG_CTRL_12			0x4D
#define IRQ_EXT_SOURCE_1		0x4E
#define IRQ_EXT_SOURCE_2		0x4F
#define IRQ_EXT_PRI_0			0x50
#define IRQ_EXT_PRI_1			0x51
#define IRQ_EXT_PRI_2			0x52
#define IRQ_STATUS			0x53
#define IRQ_SOURCE_0			0x54
#define IRQ_SOURCE_1			0x55

#define IRQ_SOURCE_2			0x56
#define HPD_PLUG_IN			BIT(0)
#define HPD_IRQ				BIT(1)
#define SOFT_INTERRUPT			BIT(2)
#define GPIO2_INTERRUPT			BIT(3)
#define USB_CABLE_PLUG			BIT(4)
#define DFP_UFP_PLUG_CHANGE		BIT(5)

#define IRQ_MASK_0			0x57
#define IRQ_MASK_1			0x58
#define IRQ_MASK_2			0x59
#define IRQ_PRI_0			0x5A
#define IRQ_PRI_1			0x5B
#define IRQ_PRI_2			0x5C
#define EE_KEY_1			0x5D
#define EE_KEY_2			0x5E
#define EE_KEY_3			0x5F

#define R_EE_CTL_1			0x60
#define R_ROM_LD_SPEED			0xC0
#define R_LOAD_EEPROM_NUM		0x3C
#define R_EE_WR_EN			BIT(0)
#define R_EE_READ_EN			BIT(1)

#define R_EE_CTL_2			0x61
#define R_EE_BURST_WR_EN		BIT(4)

#define R_EE_ADDR_L_BYTE		0x62
#define R_EE_ADDR_H_BYTE		0x63
#define R_EE_WR_DATA			0x64
#define R_EE_RD_DATA			0x65

#define R_EE_STATE			0x66
#define R_EE_RW_DONE			BIT(0)
#define R_SRAM_CRC_DONE			BIT(1)
#define R_EE_LD_DONE			BIT(3)
#define R_EE_LD_COUNTER			0xF0

#define TRY_UFP_TIMER			0x6A

#define FUNCTION_OPTION			0x6E
#define AUTO_PD_EN			BIT(1)

#define VBUS_OFF_DELAY			0x69
#define TIME_CONTROL			0x6B

#define R_EE_BURST_DATA0		0x67
#define R_EE_BURST_DATA1		0x68
#define R_EE_BURST_DATA2		0x69
#define R_EE_BURST_DATA3		0x6A
#define R_EE_BURST_DATA4		0x6B
#define R_EE_BURST_DATA5		0x6C
#define R_EE_BURST_DATA6		0x6D
#define R_EE_BURST_DATA7		0x6E
#define R_FAST_T_HD_STA			0x6F
#define R_FAST_T_SU_STA			0x70
#define R_FAST_T_HD_HIGH_BYTE		0x71
#define R_FAST_T_HD_LOW_BYTE		0x72
#define R_FAST_T_HD_DAT			0x73
#define FAST_T_SU_DAT			0x74
#define FAST_T_SU_STO			0x75
#define R_FAST_BUF			0x76
#define R_STANDARD_T_HD			0x77
#define R_STANDARD_T_SU			0x78
#define R_STANDARD_T_HIGH		0x79
#define R_STANDARD_T_LOW		0x7A
#define R_STANDARD_T_HD_DAT		0x7B
#define R_STANDARD_T_SU_DAT		0x7C
#define R_STANDARD_T_SU_STO		0x7D
#define R_STANDARD_T_BUF		0x7E

#define DEBUG_EE_0			0x7F
#define R_EE_DEBUG_STATE		0x1F

#define PD_TX_CTRL_1			0x81
#define PD_TX_CTRL_2			0x82
#define PD_TX_FIFO_WR_DATA		0x83
#define PD_TX_CLK_SEL			0x84
#define PD_TX_TRANSITION_WINDOW		0x85
#define PD_1US_PERIOD			0x86
#define PD_TX_INTER_FRAME_GAP		0x87
#define PD_TX_NUM_PREAMBLE		0x88
#define PD_TX_FIFO_STATUS		0x89
#define PD_TIMER_CTRL			0x97
#define PD_TIMER_0_VALUE		0x98
#define PD_TIMER_1_VALUE		0x99
#define PD_TX_FIFO_CTRL			0x9A
#define PD_TX_BIT_PERIOD		0x9B
#define PD_TX_AUTO_GOOD_CRC		0x9C
#define PD_TX_BIST_CTRL			0x9D
#define PD_TX_BIST_CTRL_2		0x9E
#define PD_RX_BIST_CTRL			0x9F
#define PD_TX_DEBUG			0xA0
#define PD_TIMER_2_7_0_VALUE		0xA1
#define PD_TIMER_2_15_8_VALUE		0xA2
#define PD_TIMER_3_7_0_VALUE		0xA3
#define PD_TIMER_3_15_8_VALUE		0xA4
#define PD_TIMER_3_2_19_16_VALUE	0xCD
#define PD_RX_CTRL_0			0xA5
#define PD_RX_CTRL_1			0xA6
#define PD_RX_CTRL_2			0xA7
#define PD_RX_HEADER_7_0		0xA8
#define PD_RX_HEADER_15_8		0xA9
#define PD_RX_DATA_OBJ_1_7_0		0xAA
#define PD_RX_DATA_OBJ_1_15_8		0xAB
#define PD_RX_DATA_OBJ_1_23_16		0xAC
#define PD_RX_DATA_OBJ_1_31_24		0xAD
#define PD_RX_DATA_OBJ_2_7_0		0xAE
#define PD_RX_DATA_OBJ_2_15_8		0xAF
#define PD_RX_DATA_OBJ_2_23_16		0xB0
#define PD_RX_DATA_OBJ_2_31_24		0xB1
#define PD_RX_DATA_OBJ_3_7_0		0xB2
#define PD_RX_DATA_OBJ_3_15_8		0xB3
#define PD_RX_DATA_OBJ_3_23_16		0xB4
#define PD_RX_DATA_OBJ_3_31_24		0xB5
#define PD_RX_DATA_OBJ_4_7_0		0xB6
#define PD_RX_DATA_OBJ_4_15_8		0xB7
#define PD_RX_DATA_OBJ_4_23_16		0xB8
#define PD_RX_DATA_OBJ_4_31_24		0xB9
#define PD_RX_DATA_OBJ_5_7_0		0xBA
#define PD_RX_DATA_OBJ_5_15_8		0xBB
#define PD_RX_DATA_OBJ_5_23_16		0xBC
#define PD_RX_DATA_OBJ_5_31_24		0xBD
#define PD_RX_DATA_OBJ_6_7_0		0xBE
#define PD_RX_DATA_OBJ_6_15_8		0xBF
#define PD_RX_DATA_OBJ_6_23_16		0xC0
#define PD_RX_DATA_OBJ_6_31_24		0xC1
#define PD_RX_DATA_OBJ_7_7_0		0xC2
#define PD_RX_DATA_OBJ_7_15_8		0xC3
#define PD_RX_DATA_OBJ_7_23_16		0xC4
#define PD_RX_DATA_OBJ_7_31_24		0xC5
#define PD_BIST_ERR_COUNT_7_0		0xC6
#define PD_BIST_ERR_COUNT_15_8		0xC7
#define PD_RX_HALF_BIT_MARGIN		0xC8
#define PD_RX_FULL_BIT_MARGIN		0xC9
#define PD_RX_LOC_PRE_NUM		0xCA
#define PD_RX_HALF_BIT_PERIOD		0xCB
#define PD_RX_FULL_BIT_PERIOD		0xCC

#define MAX_VOLT_RDO			0xD0
#define MAX_POWER_SYSTEM		0xD1
#define MIN_POWER_SYSTEM		0xD2

#define RDO_MAX_VOLT			0xD3
#define RDO_MAX_POWER			0xD4

#define R_OTP_ADDR_HIGH			0xD0
#define R_OTP_ADDR_LOW			0xD1
#define R_OTP_DATA_IN_0			0xD2
#define R_OTP_DATA_IN_1			0xD3
#define R_OTP_DATA_IN_2			0xD4
#define R_OTP_DATA_IN_3			0xD5
#define R_OTP_DATA_IN_4			0xD6
#define R_OTP_DATA_IN_5			0xD7
#define R_OTP_DATA_IN_6			0xD8
#define R_OTP_DATA_IN_7			0xD9
#define R_OTP_REPROG_MAX_NUM		0xDA
#define R_OTP_ECC_IN			0xDB
#define R_OTP_DATA_OUT_0		0xDC
#define R_OTP_DATA_OUT_1		0xDD
#define R_OTP_DATA_OUT_2		0xDE
#define R_OTP_DATA_OUT_3		0xDF
#define R_OTP_DATA_OUT_4		0xE0
#define R_OTP_DATA_OUT_5		0xE1
#define R_OTP_DATA_OUT_6		0xE2
#define R_OTP_DATA_OUT_7		0xE3
#define R_OTP_DATA_OUT_8		0xE4
#define R_OTP_CTL_1			0xE5
#define R_OTP_CTL_2			0xE6
#define R_OTP_DEBUG			0xE7
#define R_OTP_CODE_START_ADDR_LOW	0xE8
#define R_OTP_CODE_START_ADDR_HIGH	0xE9
#define R_OTP_TIM_PAR_1			0xEA
#define R_OTP_TIM_PAR_2			0xEB
#define R_OTP_TIM_PAR_3			0xEC

#define R_OTP_STATE_1			0xED
#define R_OTP_WRITE_WORD_STATE		0x0F
#define R_OTP_READ_WORD_STATE		0x30

#define R_OTP_TIM_PAR_4			0xEE
#define R_OTP_ACC_PROTECT		0xEF
#define R_PROBE_ADDR			0xF0
#define R_TWO_MS_L			0xF2
#define R_TWO_MS_H			0xF3
#define POINT_25_MS_L			0xF4
#define POINT_25_MS_H			0xF5
#define R_PULL_UP_DOWN_CTRL_0		0xFB

#define R_PULL_UP_DOWN_CTRL_1		0xFC
#define R_M_SCL_PULL_DOWN		BIT(0)
#define R_M_SCL_PULL_UP			BIT(1)
#define R_M_SDA_PULL_DOWN		BIT(2)
#define R_M_SDA_PULL_UP			BIT(3)
#define R_VCONN2_EN_PULL_DOWN		BIT(4)
#define R_VCONN2_EN_PULL_UP		BIT(5)
#define R_VCONN1_EN_PULL_DOWN		BIT(6)
#define R_VCONN1_EN_PULL_UP		BIT(7)

#define TENUS_TIMER_H			0xFD
#define TENUS_TIMER_L			0xFE

int anx7418_reg_init(struct anx7418 *anx);
int anx7418_pwr_on(struct anx7418 *anx, int is_on);

int anx7418_set_mode(struct anx7418 *anx, int mode);
int anx7418_set_pr(struct anx7418 *anx, int pr);
int anx7418_set_dr(struct anx7418 *anx, int dr);

#endif /* __ANX7418_H__ */

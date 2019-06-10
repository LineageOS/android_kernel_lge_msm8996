/*
 * Copyright(c) 2016, LG Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ANX7688_MI1_H
#define __ANX7688_MI1_H

/* I2C slave adress 7bit */
#define USBC_ADDR (0x50 >> 1)
#define TCPC_ADDR (0x58 >> 1)
#define DP_CORE_ADDR (0x72 >> 1)
#define DP_HDCP_ADDR (0x70 >> 1)
#define DP_ANLG_ADDR (0x7A >> 1)
#define DP_RX1_ADDR (0x7E >> 1)
#define DP_RX2_ADDR (0x80 >> 1)

/* Register map */
/* Address for 0x50 Type-C */
#define USBC_VENDORID                  0x00
#define USBC_DEVICEID                  0x02
#define USBC_DEVVER                    0x04
#define USBC_RESET_CTRL_0              0x05
#define R_PD_HARDWARE_RESET            BIT(0)
#define R_PD_RESET                     BIT(3)
#define R_OCM_RESET                    BIT(4)
#define USBC_PWRDN_CTRL                0x0D
#define R_PWRDN_PD                     BIT(0)
#define R_PWRDN_OCM                    BIT(1)
#define CC2_VRD_3P0                    BIT(2)
#define CC2_VRD_1P5                    BIT(3)
#define CC2_VRD_USB                    BIT(4)
#define CC1_VRD_3P0                    BIT(5)
#define CC1_VRD_1P5                    BIT(6)
#define CC1_VRD_USB                    BIT(7)
#define OCM_DEBUG_1                    0x12
#define OCM_DEBUG_4                    0x15
#define OCM_DEBUG_5                    0x16
#define OCM_DEBUG_9                    0x1A
#define USBC_MAX_VOLT_SET              0x1B
#define USBC_MAX_PWR_SET               0x1C
#define USBC_MIN_PWR_SET               0x1D
#define USBC_RDO_MAX_VOLT              0x1E
#define USBC_RDO_MAX_POWER             0x1F
#define USBC_INT_MASK                  0x17
#define RECVD_MSG_INT_MASK             BIT(0)
#define VCONN_CHG_INT_MASK             BIT(2)
#define VBUS_CHG_INT_MASK              BIT(3)
#define CC_STATUS_CHG_INT_MASK         BIT(4)
#define DATA_ROLE_CHG_INT_MASK         BIT(5)
#define PR_C_GOT_POWER_MASK            BIT(6)
#define USBC_VBUS_DELAY_TIME           0x22
#define USBC_TRY_UFP_TIMER             0x23
#define OCM_DEBUG_19                   0x24
#define OCM_DEBUG_20                   0x25
#define OCM_DEBUG_21                   0x26
#define USBC_AUTO_PD_MODE              0x27
#define VBUS_ADC_DISABLE               BIT(0)
#define AUTO_PD_EN                     BIT(1)
#define TRY_SRC_EN                     BIT(2)
#define TRY_SNK_EN                     BIT(3)
#define GOTO_SAFE5V_EN                 BIT(4)
#define SAFE0V_LEVEL                   (BIT(6) | BIT(7))
#define USBC_INT_STATUS                0x28
#define RECVD_MSG_INT                  BIT(0)
#define VCONN_CHG_INT                  BIT(2)
#define VBUS_CHG_INT                   BIT(3)
#define CC_STATUS_CHG_INT              BIT(4)
#define DATA_ROLE_CHG_INT              BIT(5)
#define PR_C_GOT_POWER                 BIT(6)
#define USBC_INTF_STATUS               0x29
#define IVCONN_STATUS                  BIT(2)
#define IVBUS_STATUS                   BIT(3)
#define IDATA_ROLE                     BIT(5)
#define USBC_CC_STATUS                 0x2A
#define CC_OPEN                        0x0
#define CC_VRD                         BIT(0)
#define CC_VRA                         BIT(1)
#define CC_RPUSB                       BIT(2)
#define CC_RP1P5                       BIT(3)
#define CC_RP3P0                       (BIT(2) | BIT(3))
#define USBC_INTP_CTRL                 0x33
#define USBC_IRQ_EXT_MASK_2            0x3D
#define USBC_ANALOG_STATUS             0x40
#define CABLE_DETECT                   BIT(0)
#define R_TRY_DRP                      BIT(1)
#define CC1_DISCHARGE                  BIT(2)
#define DFP_OR_UFP                     BIT(3)
#define UFP_PLUG                       BIT(4)
#define PIN_VCONN_1_IN                 BIT(5)
#define PIN_VCONN_2_IN                 BIT(6)
#define CC2_DISCHARGE                  BIT(7)
#define USBC_ANALOG_CTRL_0             0x41
#define USBC_ANALOG_CTRL_2             0x43
#define USBC_ANALOG_CTRL_4             0x45
#define PWR_SW_ON_CC1                  BIT(6)
#define PWR_SW_ON_CC2                  BIT(7)
#define USBC_ANALOG_CTRL_6             0x47
#define USBC_R_RP                      (BIT(0) | BIT(1))
#define USBC_ANALOG_CTRL_7             0x48
#define CC2_RD_CONNECT                 BIT(0)
#define CC2_RA_CONNECT                 (BIT(0) | BIT(1))
#define CC1_RD_CONNECT                 BIT(2)
#define CC1_RA_CONNECT                 (BIT(2) | BIT(3))
#define USBC_ANALOG_CTRL_9             0x4A
#define CC_SOFT_EN                     BIT(0)
#define BMC_ON_CC1                     BIT(1)
#define USBC_IRQ_EXT_SOURCE_2          0x4F
#define IRQ_EXT_SOFT_RESET_BIT         BIT(2)
#define USBC_EE_KEY_1                  0x3F
#define USBC_EE_KEY_2                  0x44
#define USBC_EE_KEY_3                  0x66
#define USBC_T_TIME_1                  0x6C
#define USBC_FUNC_OPT                  0x6E
#define SAR_ADC_CTRL                   0x74
#define R_ADC_EN                       BIT(1)
#define R_ADC_VBUS_SEL                 BIT(2)
#define R_ADC_VCONN_SEL                BIT(3)
#define R_ADC_READY                    BIT(4)
#define R_ADC_OUT8                     BIT(5)
#define SAR_ADC_OUT                    0x75

#define EEPROM_WR_DATA0                0xD0
#define EEPROM_WR_DATA1                0xD1
#define EEPROM_WR_DATA2                0xD2
#define EEPROM_WR_DATA3                0xD3
#define EEPROM_WR_DATA4                0xD4
#define EEPROM_WR_DATA5                0xD5
#define EEPROM_WR_DATA6                0xD6
#define EEPROM_WR_DATA7                0xD7
#define EEPROM_WR_DATA8                0xD8
#define EEPROM_WR_DATA9                0xD9
#define EEPROM_WR_DATA10               0xDA
#define EEPROM_WR_DATA11               0xDB
#define EEPROM_WR_DATA12               0xDC
#define EEPROM_WR_DATA13               0xDD
#define EEPROM_WR_DATA14               0xDE
#define EEPROM_WR_DATA15               0xDF

#define EEPROM_ADDR_H                  0xE0
#define EEPROM_ADDR_L                  0xE1
#define EEPROM_WR_ENABLE               0xE2

/* Address for 0x58 TCPC */

/* Status Machine */
#define STATE_DISABLED                 0x00
#define STATE_ERROR_RECOVERY           0x01
#define STATE_UNATTACHED_DRP           0x02
#define STATE_UNATTACHED_SNK           0x03
#define STATE_UNATTACHED_SRC           0x04
#define STATE_ATTACHWAIT_SNK           0x05
#define STATE_ATTACHWAIT_SRC           0x06
#define STATE_ATTACHED_SNK             0x07
#define STATE_ATTACHED_SRC             0x08
#define STATE_AUDIO_ACCESSORY          0x09
#define STATE_DEBUG_ACCESSORY          0x0A
#define STATE_PWRED_ACCESSORY          0x0B
#define STATE_PWRED_NOSINK             0x0C
#define STATE_TRY_SNK                  0x0D
#define STATE_TRYWAIT_SNK              0x0E
#define STATE_TRY_SRC                  0x0F
#define STATE_TRYWAIT_SRC              0x10

#define ANX_ROLE_SNK                   BIT(0)
#define ANX_ROLE_SNK_ACC               BIT(1)
#define ANX_ROLE_SRC_OR_SNK            BIT(2)
#define ANX_ROLE_SRC                   BIT(3)
#define ANX_ROLE_DRP                   BIT(4)

#define USBC_UNKNWON_CHARGER           BIT(0)
#define USBC_CHARGER                   BIT(1)
#define USBC_PD_CHARGER                BIT(2)
#define USBC_VOLT_RP3P0                5000
#define USBC_VOLT_RP1P5                5000
#define USBC_VOLT_RPUSB                5000
#define USBC_CURR_RP3P0                3000
#define USBC_CURR_RP1P5                1500
#define USBC_CURR_RPUSB                500
#define USBC_CURR_RESTRICT             1000

#ifdef CONFIG_LGE_DP_ANX7688
#define ANX_DP_CONNECTED               BIT(2)
#define ANX_DP_RUNNING                 BIT(1)
#define ANX_DP_DISCONNECTED            BIT(0)
#endif /* CONFIG_LGE_DP_ANX7688 */

#endif /* __ANX7688_MI1_H */

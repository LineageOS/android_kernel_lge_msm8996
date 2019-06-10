#ifndef __ANX7418_PD_H__
#define __ANX7418_PD_H__

#include <linux/types.h>
#include <linux/i2c.h>

#define PD_SEND_TIMEOUT msecs_to_jiffies(32)
#define PD_RECV_TIMEOUT msecs_to_jiffies(32)

#define PD_VOLT(mv)			((mv) / 50U)
#define PD_VOLT_GET(mv)			((mv) * 50U)
#define PD_CURR(ma)			((ma) / 10U)
#define PD_CURR_GET(ma)			((ma) * 10U)
#define PD_POWER(power)			((power) / 250U)
#define PD_POWER_GET(power)		((power) * 250U)

typedef enum {
	PD_TYPE_PWR_SRC_CAP = 0x00,
	PD_TYPE_PWR_SNK_CAP = 0x01,
	PD_TYPE_DP_SNK_IDENTITY = 0x02,
	PD_TYPE_SVID = 0x03,
	PD_TYPE_GET_DP_SNK_CFG = 0x04,
	PD_TYPE_ACCEPT = 0x05,
	PD_TYPE_REJECT = 0x06,
	PD_TYPE_PSWAP_REQ = 0x10,
	PD_TYPE_DSWAP_REQ = 0x11,
	PD_TYPE_GOTO_MIN_REQ = 0x12,
	PD_TYPE_VCONN_SWAP_REQ = 0x13,
	PD_TYPE_VDM = 0x14,
	PD_TYPE_DP_SNK_CFG = 0x15,
	PD_TYPE_PWR_OBJ_REQ = 0x16,
	PD_TYPE_PD_STATUS_REQ = 0x17,
	PD_TYPE_DP_ALT_ENTER = 0x19,
	PD_TYPE_DP_ALT_EXIT = 0x1A,
	PD_TYPE_RESPONSE_TO_REQ = 0xF0,
	PD_TYPE_SOFT_RST = 0xF1,
	PD_TYPE_HARD_RST = 0xF2,
	PD_TYPE_RESTART = 0xF3,
} pd_type;

typedef struct {
	u8 length;
	u8 type;
} pd_msg_hdr_t;

typedef struct {
	pd_msg_hdr_t hdr;
	u8 data[30];
} pd_msg_t;

/* PDO : Power Data Object */
typedef enum {
	PDO_TYPE_FIXED = 0x00,
	PDO_TYPE_BATTERY,
	PDO_TYPE_VARIABLE,
} pdo_type;

typedef struct {
	u32 curr:10;			// Maximum Current in 10mA units
	u32 volt:10;			// Voltage in 50mV units
	u32 peek_curr:2;		// Peak Current
	u32 reserved:3;
	u32 data_swap:1;		// Data Role Swap
	u32 comm_cap:1;			// USB Communications Capable
	u32 external:1;			// Externally Powered
	u32 suspend:1;			// USB Suspend Supported
	u32 dual_role:1;		// Dual-Role Power
	u32 type:2;			// Fixed supply
} pdo_fixed_t;

typedef struct {
	u32 max_power:10;		// Maximum Allowable Power in 250mW units
	u32 min_volt:10;		// Minimum Voltage in 50mV units
	u32 max_volt:10;		// Maximum Voltage in 50mV units
	u32 type:2;			// Battery
} pdo_battery_t;

typedef struct {
	u32 max_curr:10;		// Maximum Current in 10mA units
	u32 min_volt:10;		// Minimum Voltage in 50mV units
	u32 max_volt:10;		// Maximum Voltage in 50mV units
	u32 type:2;			// Variable Supply (non-battery)
} pdo_variable_t;

typedef union {
	struct {
		u32 reserved:30;
		u32 type:2;
	} common;
	pdo_fixed_t fixed;
	pdo_battery_t battery;
	pdo_variable_t variable;
} pdo_t;

/* RDO : Request Data Object */
typedef struct {
	u32 max_curr:10;		// Maximum Operating Current 10mA units
	u32 op_curr:10;			// Operating current in 10mA units
	u32 reserved1:4;		// Reserved - shall be set to zero.
	u32 no_suspend:1;		// No USB Suspend
	u32 comm_cap:1;			// USB Communications Capable
	u32 cap_mismatch:1;		// Capability Mismatch
	u32 give_back:1;		// GiveBack flag
	u32 pos:3;			// Object position
					// (000b is Reserved and shall not be used)
	u32 reserved2:1;		// Reserved - shall be set to zero
} rdo_fixed_t;
typedef rdo_fixed_t rdo_variable_t;

typedef struct {
	u32 max_power:10;		// Maximum Operating Power in 250mW units
	u32 op_power:10;		// Operating Power in 250mW units
	u32 reserved1:4;		// Reserved - shall be set to zero.
	u32 no_suspend:1;		// No USB Suspend
	u32 comm_cap:1;			// USB Communications Capable
	u32 cap_mismatch:1;		// Capability Mismatch
	u32 give_back:1;		// GiveBack flag
	u32 pos:3;			// Object position
					// (000b is Reserved and shall not be used)
	u32 reserved2:1;		// Reserved - shall be set to zero
} rdo_battery_t;

typedef union {
	struct {
		u32 max:10;
		u32 op:10;
		u32 reserved1:4;	// Reserved - shall be set to zero.
		u32 no_suspend:1;	// No USB Suspend
		u32 comm_cap:1;		// USB Communications Capable
		u32 cap_mismatch:1;	// Capability Mismatch
		u32 give_back:1;	// GiveBack flag
		u32 pos:3;		// Object position
					// (000b is Reserved and shall not be used)
		u32 reserved2:1;	// Reserved - shall be set to zero
	} common;
	rdo_fixed_t fixed;
	rdo_battery_t battery;
	rdo_variable_t variable;
} rdo_t;

/* Response */
typedef enum {
	RESPONSE_STATUS_SUCCESS = 0x00,
	RESPONSE_STATUS_REJECT = 0x01,
	RESPONSE_STATUS_FAILURE = 0x02,
	RESPONSE_STATUS_BUSY = 0x03
} response_status_t;

typedef struct {
	u8 req_type;
	u8 status;
} response_t;

int anx7418_pd_init(struct anx7418 *anx);
int anx7418_pd_src_cap_init(struct anx7418 *anx);
int anx7418_pd_process(struct anx7418 *anx);

int anx7418_send_pd_msg(struct i2c_client *client, u8 type,
		const u8 *buf, size_t len, unsigned long timeout);
int anx7418_recv_pd_msg(struct i2c_client *client,
		u8 *buf, size_t len, unsigned long timeout);

#endif /* __ANX7418_PD_H__ */

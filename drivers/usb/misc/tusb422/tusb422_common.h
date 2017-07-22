/*
 * Constants for the Texas Instruments TUSB422 Power Delivery
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * Copyright: (C) 2016 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __TUSB422_COMMON_H__
#define __TUSB422_COMMON_H__

#ifdef CONFIG_TUSB422
	#include "tusb422_linux.h"
#else
	#include "uart.h"
	#include <stdbool.h>
	#include <stdint.h>
#endif

#ifdef CONFIG_LGE_USB_TYPE_C
#include <linux/ratelimit.h>
#endif

/** Debug print functions **/
#ifndef DEBUG_LEVEL
	#define DEBUG_LEVEL 1
#endif

#ifdef CONFIG_TUSB422
#define PRINT_PREFIX "TUSB422 "
#else
#define PRINT_PREFIX ""
#endif

#if DEBUG_LEVEL >= 1
// Variadic macro requires "--gcc" compiler switch.
#ifdef CONFIG_LGE_USB_TYPE_C
    #define PRINT(str, args...)  printk_ratelimited(PRINT_PREFIX str, ##args)
#else
    #define PRINT(str, args...)  printk(PRINT_PREFIX str, ##args)
#endif
#else
#ifdef CONFIG_LGE_USB_TYPE_C
	#define PRINT(str, args...)  pr_debug_ratelimited(PRINT_PREFIX str, ##args)
#else
	#define PRINT(str, args...)  {}
#endif
#endif

#if DEBUG_LEVEL >= 2
// Variadic macro requires "--gcc" compiler switch.
#ifdef CONFIG_LGE_USB_TYPE_C
	#define CRIT(str, args...)  printk_ratelimited(PRINT_PREFIX str, ##args)
#else
	#define CRIT(str, args...)  printk(PRINT_PREFIX str, ##args)
#endif
#else
#ifdef CONFIG_LGE_USB_TYPE_C
	#define CRIT(str, args...)  pr_debug_ratelimited(PRINT_PREFIX str, ##args)
#else
	#define CRIT(str, args...)  {}
#endif
#endif

#if DEBUG_LEVEL >= 3
// Variadic macro requires "--gcc" compiler switch.
#ifdef CONFIG_LGE_USB_TYPE_C
	#define DEBUG(str, args...)  printk_ratelimited(PRINT_PREFIX str, ##args)
#else
	#define DEBUG(str, args...)  printk(PRINT_PREFIX str, ##args)
#endif
#else
#ifdef CONFIG_LGE_USB_TYPE_C
	#define DEBUG(str, args...)  pr_debug_ratelimited(PRINT_PREFIX str, ##args)
#else
	#define DEBUG(str, args...)  {}
#endif
#endif

#if DEBUG_LEVEL >= 4
// Variadic macro requires "--gcc" compiler switch.
#ifdef CONFIG_LGE_USB_TYPE_C
	#define INFO(str, args...)  printk_ratelimited(PRINT_PREFIX str, ##args)
#else
	#define INFO(str, args...)  printk(PRINT_PREFIX str, ##args)
#endif
#else
#ifdef CONFIG_LGE_USB_TYPE_C
	#define INFO(str, args...)  pr_debug_ratelimited(PRINT_PREFIX str, ##args)
#else
	#define INFO(str, args...)  {}
#endif
#endif

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))


typedef enum
{
	SMBUS_MASTER0 = 0,
	TOTAL_NUM_SMBUS_MASTERS
} smbus_interface_t;

typedef enum
{
	ROLE_SRC = 0,
	ROLE_SNK,	  /* Accessories are not supported */
	ROLE_DRP	  /* Dual Role Power */
} tc_role_t;

typedef enum
{
	RP_DEFAULT_CURRENT = 0,
	RP_MEDIUM_CURRENT,	/* 1.5A */
	RP_HIGH_CURRENT		/* 3.0A */
} tcpc_role_rp_val_t;

typedef struct
{
	smbus_interface_t  intf;
	uint8_t            slave_addr;
	uint16_t           flags;
	tc_role_t          role;
	tcpc_role_rp_val_t rp_val;

} tcpc_config_t;

/** Power Control functions **/
enum vbus_select_t
{
	VBUS_SRC_5V       = (1 << 0),
	VBUS_SRC_HI_VOLT  = (1 << 1),
	VBUS_SNK          = (1 << 2)
};

enum supply_type_t
{
	SUPPLY_TYPE_FIXED    = 0,
	SUPPLY_TYPE_BATTERY  = 1,
	SUPPLY_TYPE_VARIABLE = 2,
};

enum peak_current_t
{
	PEAK_CURRENT_0 = 0,	  /*! Peak current = Ioc default  */
	PEAK_CURRENT_1 = 1,	  /*! Peak current = 110% Ioc for 10ms */
	PEAK_CURRENT_2 = 2,	  /*! Peak current = 125% Ioc for 10ms */
	PEAK_CURRENT_3 = 3	  /*! Peak current = 150% Ioc for 10ms */
};

struct src_pdo_t
{
	enum supply_type_t SupplyType; /*! Supply Type     (all types)         */
	enum peak_current_t PeakI;	   /*! Peak Current    (fixed, varible)    */
	uint16_t        MinV;		   /*! Minimum Voltage (all types)         */
	uint16_t        MaxV;		   /*! Maximum Voltage (variable, battery) */
	uint16_t        MaxI;		   /*! Maximum Current (fixed, variable)   */
	uint16_t        MaxPower;	   /*! Maximum Power   (battery)           */
};

struct snk_pdo_t
{
	enum supply_type_t   SupplyType;	 /*! Supply Type     (all types)         */
	uint16_t        MinV;				 /*! Minimum Voltage (all types)         */
	uint16_t        MaxV;				 /*! Maximum Voltage (variable, battery) */
	uint16_t        MaxOperatingCurrent; /*! Maximum Current (fixed, variable)   */
	uint16_t        MinOperatingCurrent; /*! Mininum Current (fixed, variable)   */
	uint16_t        OperationalCurrent;	 /*! Current         (fixed, variable)   */
	uint16_t        MaxOperatingPower;	 /*! Maximum Power   (battery)           */
	uint16_t        MinOperatingPower;	 /*! Minimum Power   (battery)           */
	uint16_t        OperationalPower;	 /*! Power           (battery)           */
};

#define PD_MAX_PDO_NUM   6
#define PDO_VOLT(mv)   ((uint16_t)((mv)/50))    // 50mV LSB
#define PDO_CURR(ma)   ((uint16_t)((ma)/10))    // 10mA LSB
#define PDO_PWR(mw)    ((uint16_t)((mw)/250))   // 250mW LSB

typedef enum
{
	FR_SWAP_NOT_SUPPORTED = 0,
	FR_SWAP_DEFAULT_PWR,
	FR_SWAP_1P5_AMP,
	FR_SWAP_3P0_AMP
} fr_swap_current_t;

typedef enum
{
	PRIORITY_VOLTAGE = 0,
	PRIORITY_CURRENT,
	PRIORITY_POWER
} pdo_priority_t;

typedef struct
{
	// BQ - todo - convert option bools to single uint32_t with bit flags

	/* General */
	bool            usb_comm_capable;
	bool            externally_powered;
	bool            dual_role_data;
	bool            unchunked_msg_support;	/*! always false for TUSB422 */

	/* Source Caps */
	bool            usb_suspend_supported;
	uint8_t         num_src_pdos;
	struct src_pdo_t       src_caps[PD_MAX_PDO_NUM];
	uint16_t        src_settling_time_ms;	  /*! Power supply settling time in ms */

	/* Sink Caps */
	uint8_t         num_snk_pdos;
	struct snk_pdo_t       snk_caps[PD_MAX_PDO_NUM];
	bool            higher_capability;
	bool            giveback_flag;
	bool            no_usb_suspend;
	fr_swap_current_t     fast_role_swap_support;
	pdo_priority_t  pdo_priority;

	bool            auto_accept_swap_to_dfp;
	bool            auto_accept_swap_to_ufp;
	bool            auto_accept_swap_to_source;
	bool            auto_accept_swap_to_sink;
	bool            auto_accept_vconn_swap;

} usb_pd_port_config_t;

/** I2C Access functions **/

// Type-C port maps to an I2C master and I2C slave address of the TCPC device.
void tcpc_config(unsigned int port, smbus_interface_t intf, uint8_t slave_addr);


//*****************************************************************************
//
//! \brief   I2C command that reads 8 bit TCPC register
//!
//! \param   port - Type-C port number.
//! \param   reg - TCPC register
//! \param   data - pointer to the variable to read the data into
//!
//! \return  USB_PD_RET_OK
//
// *****************************************************************************
int8_t tcpc_read8(unsigned int port, uint8_t reg, uint8_t *data);

//*****************************************************************************
//
//! \brief   I2C command that reads 16 bit TCPC register
//!
//! \param   port - Type-C port number.
//! \param   reg - TCPC register
//! \param   data - pointer to the variable to read the data into
//!
//! \return  USB_PD_RET_OK
//
// *****************************************************************************
int8_t tcpc_read16(unsigned int port, uint8_t reg, uint16_t *data);


//*****************************************************************************
//
//! \brief   I2C command that reads block of data.
//!
//! \param   port - Type-C port number.
//! \param   reg - TCPC register
//! \param   data - pointer to the variable to read the data into
//! \param   len - data length
//!
//! \return  USB_PD_RET_OK
//
// *****************************************************************************
int8_t tcpc_read_block(unsigned int port, uint8_t reg, uint8_t *data,
					   unsigned int len);

//*****************************************************************************
//
//! \brief   I2C command that writes data into an 8-bit TCPC register
//!
//! \param   port - Type-C port number.
//! \param   reg - TCPC register
//! \param   data - data to be written
//!
//! \return  USB_PD_RET_OK
//
// *****************************************************************************
int8_t tcpc_write8(unsigned int port, uint8_t reg, uint8_t data);

//*****************************************************************************
//
//! \brief   I2C command that writes data into an 16 bit TCPC register
//!
//! \param   port - Type-C port number.
//! \param   reg - TCPC register
//! \param   data - data to be written
//!
//! \return  USB_PD_RET_OK
//
// *****************************************************************************
int8_t tcpc_write16(unsigned int port, uint8_t reg, uint16_t data);


//*****************************************************************************
//
//! \brief   I2C command that writes data block the TX_CTRL register
//!
//! \param   port - Type-C port number.
//! \param   reg - TCPC register
//! \param   data - pointer to the variable that contains the data to be written
//! \param   len - data length
//!
//! \return  USB_PD_RET_OK
//
// *****************************************************************************
int8_t tcpc_write_block(unsigned int port,
						uint8_t reg,
						uint8_t *data,
						uint8_t len);

// Modifies an 8-bit register.
void tcpc_modify8(unsigned int port,
				  uint8_t reg,
				  uint8_t clr_mask,
				  uint8_t set_mask);

// Modifies an 16-bit register.
void tcpc_modify16(unsigned int port,
				   uint8_t reg,
				   uint16_t clr_mask,
				   uint16_t set_mask);



/** TIMERS **/
struct tusb422_timer_t
{
	uint32_t      expires;
	void          (*function)(unsigned int);
	unsigned int  data;
	bool          queued;
};

//*****************************************************************************
//
//! \brief Starts a timer.
//!
//! \param timer        pointer to timer struct.
//! \param timeout_ms   time in milliseconds
//! \param function     pointer to function to call upon expiration
//!
//! \return timer list index or -1 if no timers available
//
// ****************************************************************************
int timer_start(struct tusb422_timer_t *timer, unsigned int timeout_ms, void (*function)(unsigned int));

//*****************************************************************************
//
//! \brief Cancels a timer.
//!
//! \param timer pointer to timer
//
// ****************************************************************************
void timer_cancel(struct tusb422_timer_t *timer);

// These functions will control platform-specific GPIOs to sink or source VBUS.
// Cannot make these more generic since power controls may be active high/low or
// open drain and require multiple GPIOs.
void tcpm_hal_vbus_enable(uint8_t port, enum vbus_select_t sel);
void tcpm_hal_vbus_disable(uint8_t port, enum vbus_select_t sel);
void tcpm_msleep(int msecs);

int tcpm_init(const tcpc_config_t *config);
void usb_pd_init(const usb_pd_port_config_t *port_config);
void usb_pd_print_version(void);

void tcpm_connection_state_machine(unsigned int port);
void usb_pd_pe_state_machine(unsigned int port);
bool tcpm_alert_event(unsigned int port);

#endif //__TUSB422_COMMON_H__

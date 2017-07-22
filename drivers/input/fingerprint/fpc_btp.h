/* FPC1021 Area sensor driver
 *
 * Copyright (c) 2013 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#ifndef LINUX_SPI_FPC_BTP_H
#define LINUX_SPI_FPC_BTP_H

struct fpc_btp_platform_data {
	int irq_gpio;
	int reset_gpio;
	int cs_gpio;
	int ldo_en;
	int qup_id;
	int use_regulator;
	struct regulator *vreg;
};

#endif


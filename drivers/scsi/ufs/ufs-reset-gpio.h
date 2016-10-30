/*
 * Copyright (c) 2016 -, Linux Foundation. All rights reserved.
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

#ifndef UFS_RESET_GPIO_H
#define UFS_RESET_GPIO_H

int ufs_card_reset(struct ufs_hba *hba, bool async);
int ufs_card_reset_init(struct ufs_hba *hba);
int ufs_card_reset_enable(struct ufs_hba *hba, bool enable);

#ifdef CONFIG_UFS_LGE_CARD_RESET_DEBUGFS
void ufs_card_reset_add_debugfs(struct ufs_hba *hba);
#endif

#endif

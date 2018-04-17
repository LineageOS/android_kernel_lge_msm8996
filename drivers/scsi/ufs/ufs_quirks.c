/*
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ufshcd.h"
#include "ufs_quirks.h"


static struct ufs_card_fix ufs_fixups[] = {
	/* UFS cards deviations table */
#ifdef CONFIG_MACH_LGE
	/* LGE_CHANGE, 2015/11/26, H1-BSP-FS@lge.com
	 * Toshiba recommend that we should not use FASTAUTO in Toshiba-UFS-Gen3.
	 */
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G8J4LBATR", UFS_DEVICE_NO_FASTAUTO),
#endif
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL, UFS_DEVICE_NO_VCCQ),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_NO_FASTAUTO),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9C8KBADG",
		UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9D8KBADG",
		UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE),
#ifdef CONFIG_MACH_LGE
	UFS_FIX(UFS_ANY_VENDOR, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_SAVECONFIGTIME),
#else
	UFS_FIX(UFS_VENDOR_HYNIX, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_SAVECONFIGTIME),
#endif

	END_FIX
};

static int ufs_get_device_info(struct ufs_hba *hba,
				struct ufs_card_info *card_data)
{
	int err;
	u8 model_index;
	u8 str_desc_buf[QUERY_DESC_STRING_MAX_SIZE + 1];
	u8 desc_buf[QUERY_DESC_DEVICE_MAX_SIZE];

	err = ufshcd_read_device_desc(hba, desc_buf,
					QUERY_DESC_DEVICE_MAX_SIZE);
	if (err)
		goto out;

	/*
	 * getting vendor (manufacturerID) and Bank Index in big endian
	 * format
	 */
	card_data->wmanufacturerid = desc_buf[DEVICE_DESC_PARAM_MANF_ID] << 8 |
				     desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];

	model_index = desc_buf[DEVICE_DESC_PARAM_PRDCT_NAME];

	memset(str_desc_buf, 0, QUERY_DESC_STRING_MAX_SIZE);
	err = ufshcd_read_string_desc(hba, model_index, str_desc_buf,
					QUERY_DESC_STRING_MAX_SIZE, ASCII_STD);
	if (err)
		goto out;

	str_desc_buf[QUERY_DESC_STRING_MAX_SIZE] = '\0';
	strlcpy(card_data->model, (str_desc_buf + QUERY_DESC_HDR_SIZE),
		min_t(u8, str_desc_buf[QUERY_DESC_LENGTH_OFFSET],
		      MAX_MODEL_LEN));
	/* Null terminate the model string */
	card_data->model[MAX_MODEL_LEN] = '\0';

out:
	return err;
}

void ufs_advertise_fixup_device(struct ufs_hba *hba)
{
	int err;
	struct ufs_card_fix *f;
	struct ufs_card_info card_data;

	card_data.wmanufacturerid = 0;
	card_data.model = kmalloc(MAX_MODEL_LEN + 1, GFP_KERNEL);
	if (!card_data.model)
		goto out;

	/* get device data*/
	err = ufs_get_device_info(hba, &card_data);
	if (err) {
		dev_err(hba->dev, "%s: Failed getting device info\n", __func__);
		goto out;
	}

	for (f = ufs_fixups; f->quirk; f++) {
		/* if same wmanufacturerid */
		if (((f->card.wmanufacturerid == card_data.wmanufacturerid) ||
		     (f->card.wmanufacturerid == UFS_ANY_VENDOR)) &&
		    /* and same model */
		    (STR_PRFX_EQUAL(f->card.model, card_data.model) ||
		     !strcmp(f->card.model, UFS_ANY_MODEL)))
#ifdef CONFIG_MACH_LGE
		    {
			    /* update quirks */
			    dev_err(hba->dev, "[LGE][UFS] update quirks, manufacturerid:%d, model:%s, quirk:0x%x\n", f->card.wmanufacturerid, f->card.model, f->quirk);
			    hba->dev_quirks |= f->quirk;
		    }
#else
			/* update quirks */
			hba->dev_quirks |= f->quirk;
#endif
	}
out:
	kfree(card_data.model);
}

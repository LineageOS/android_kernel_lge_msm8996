/*
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012 Synaptics, Inc.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom
   the Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.


   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
#define SYNA_F34_SAMPLE_CODE
#define SHOW_PROGRESS

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>

#include <linux/input/lge_touch_core.h>
#include <linux/input/touch_synaptics.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>


/* Variables for F34 functionality */
unsigned short SynaF34DataBase;
unsigned short SynaF34QueryBase;
unsigned short SynaF01DataBase;
unsigned short SynaF01CommandBase;
unsigned short SynaF01QueryBase;

unsigned short SynaF34Reflash_BlockNum;
unsigned short SynaF34Reflash_BlockData;
unsigned short SynaF34ReflashQuery_BootID;
unsigned short SynaF34ReflashQuery_FlashPropertyQuery;
unsigned short SynaF34ReflashQuery_BlockSize;
unsigned short SynaF34ReflashQuery_FirmwareBlockCount;
unsigned short SynaF34ReflashQuery_ConfigBlockCount;

unsigned char SynaF01Query43Length;

unsigned short SynaFirmwareBlockSize;
unsigned short SynaFirmwareBlockCount;
unsigned long SynaImageSize;

unsigned short SynaConfigBlockSize;
unsigned short SynaConfigBlockCount;
unsigned long SynaConfigImageSize;

unsigned short SynaBootloadID;

unsigned short SynaF34_FlashControl;
unsigned short SynaF34_FlashStatus;

unsigned char *SynafirmwareImgData;
unsigned char *SynaconfigImgData;
unsigned char *SynalockImgData;
unsigned int SynafirmwareImgVersion;

unsigned char *my_image_bin;
unsigned long my_image_size;
u8 fw_image_config_id[5];

unsigned char *ConfigBlock;

static void CompleteReflash(struct synaptics_ts_data *ts);
static void SynaInitialize(struct synaptics_ts_data *ts);
static void SynaReadFirmwareInfo(struct synaptics_ts_data *ts);
static void SynaEnableFlashing(struct synaptics_ts_data *ts);
static void SynaProgramFirmware(struct synaptics_ts_data *ts);
static void SynaFinalizeReflash(struct synaptics_ts_data *ts);
static unsigned int SynaWaitForATTN(int time, struct synaptics_ts_data *ts);
static bool CheckTouchControllerType(struct synaptics_ts_data *ts);
static void eraseAllBlock(struct synaptics_ts_data *ts);
static void SynaUpdateConfig(struct synaptics_ts_data *ts);
static void EraseConfigBlock(struct synaptics_ts_data *ts);


#define BL7
#ifdef BL7
static void CompleteReflash_BL7(struct synaptics_ts_data *ts);
#endif

enum FlashCommand {
	/* prior to V2 bootloaders */
	m_uF34ReflashCmd_FirmwareCrc        = 0x01,
	m_uF34ReflashCmd_FirmwareWrite      = 0x02,
	m_uF34ReflashCmd_EraseAll           = 0x03,
	/* V2 and later bootloaders */
	m_uF34ReflashCmd_LockDown           = 0x04,
	m_uF34ReflashCmd_ConfigRead         = 0x05,
	m_uF34ReflashCmd_ConfigWrite        = 0x06,
	m_uF34ReflashCmd_EraseUIConfig      = 0x07,
	m_uF34ReflashCmd_Enable             = 0x0F,
	m_uF34ReflashCmd_QuerySensorID      = 0x08,
	m_uF34ReflashCmd_EraseBLConfig      = 0x09,
	m_uF34ReflashCmd_EraseDisplayConfig = 0x0A,
};

char SynaFlashCommandStr[0x0C][0x20] = {
	"",
	"FirmwareCrc",
	"FirmwareWrite",
	"EraseAll",
	"LockDown",
	"ConfigRead",
	"ConfigWrite",
	"EraseUIConfig",
	"Enable",
	"QuerySensorID",
	"EraseBLConfig",
	"EraseDisplayConfig",
};

int FirmwareUpgrade(struct synaptics_ts_data *ts,
		const struct firmware *fw_entry)
{
	int ret = 0;

	my_image_size = fw_entry->size;
	my_image_bin = vmalloc(sizeof(char) * (my_image_size + 1));

	if (my_image_bin == NULL) {
		TOUCH_E("Can not allocate  memory\n");
		ret = -ENOMEM;
		goto error;
	}

	memcpy(my_image_bin, fw_entry->data, my_image_size);

	/* for checksum */
	*(my_image_bin+my_image_size) = 0xFF;

	strlcpy(ts->fw_info.img_product_id,
			&my_image_bin[ts->pdata->fw_pid_addr], 6);
	strlcpy(ts->fw_info.img_version,
			&my_image_bin[ts->pdata->fw_ver_addr], 4);

	ts->fw_info.fw_start = (unsigned char *)&my_image_bin[0];
	ts->fw_info.fw_size = my_image_size;

	if (ts->pdata->role->fw_index == BL_VER_HIGHER) {
		TOUCH_I("[%s] CompleteReflash_BL7 Start\n", __func__);
		CompleteReflash_BL7(ts);
	} else if (ts->pdata->role->fw_index == BL_VER_LOWER) {
		TOUCH_I("[%s] CompleteReflash Start\n", __func__);
		CompleteReflash(ts);
	} else {
		TOUCH_I("[%s] No Bootloader Type\n", __func__);
	}

	return ret;
error:
	memset(&fw_entry, 0, sizeof(fw_entry));
	return ret;
}



static int writeRMI(struct i2c_client *client,
		u8 uRmiAddress, u8 *data, unsigned int length)
{
	return touch_ts_i2c_write(client, uRmiAddress, length, data);
}

static int readRMI(struct i2c_client *client,
		u8 uRmiAddress, u8 *data, unsigned int length)
{
	return touch_ts_i2c_read(client, uRmiAddress, length, data);
}

/* no ds4 */
bool CheckFlashStatus(struct synaptics_ts_data *ts,
		enum FlashCommand command)
{
	unsigned char uData = 0;
	/*
	 * Read the "Program Enabled" bit of the F34 Control register,
	 * and proceed only if the bit is set.
	 */
	readRMI(ts->client, SynaF34_FlashStatus, &uData, 1);

	return !(uData & 0x3F);
}

/* no ds4 */
void SynaImageParser(struct synaptics_ts_data *ts)
{
	/* img file parsing */
	SynaImageSize = ((unsigned int)my_image_bin[0x08] |
			(unsigned int)my_image_bin[0x09] << 8 |
			(unsigned int)my_image_bin[0x0A] << 16 |
			(unsigned int)my_image_bin[0x0B] << 24);
	SynafirmwareImgData = (unsigned char *)((&my_image_bin[0]) + 0x100);
	SynaconfigImgData  =
		(unsigned char *)(SynafirmwareImgData + SynaImageSize);
	SynafirmwareImgVersion = (unsigned int)(my_image_bin[7]);

	switch (SynafirmwareImgVersion) {
	case 2:
		SynalockImgData = (unsigned char *)((&my_image_bin[0]) + 0xD0);
		break;
	case 3:
	case 4:
		SynalockImgData = (unsigned char *)((&my_image_bin[0]) + 0xC0);
		break;
	case 5:
	case 6:
		SynalockImgData = (unsigned char *)((&my_image_bin[0]) + 0xB0);
	default:
		break;
	}
}

/* no ds4 */
void SynaBootloaderLock(struct synaptics_ts_data *ts)
{
	unsigned short lockBlockCount;
	unsigned char uData[2] = {0};
	unsigned short uBlockNum;
	enum FlashCommand cmd;

	if (my_image_bin[0x1E] == 0) {
		TOUCH_E("Skip lockdown process with this .img\n");
		return;
	}

	/* Check if device is in unlocked state */
	readRMI(ts->client, (SynaF34QueryBase + 1), &uData[0], 1);

	/* Device is unlocked */
	if (uData[0] & 0x02) {
		TOUCH_E("Device unlocked. Lock it first...\n");
		/*
		 * Different bootloader version has different block count
		 * for the lockdown data
		 * Need to check the bootloader version from the image file
		 * being reflashed
		 */
		switch (SynafirmwareImgVersion) {
		case 2:
			lockBlockCount = 3;
			break;
		case 3:
		case 4:
			lockBlockCount = 4;
			break;
		case 5:
		case 6:
			lockBlockCount = 5;
			break;
		default:
			lockBlockCount = 0;
			break;
		}

		/* Write the lockdown info block by block */
		/* This reference code of lockdown process does not check */
		/* for bootloader version */
		/* currently programmed on the ASIC against the bootloader */
		/* version of the image to */
		/* be reflashed. Such case should not happen in practice. */
		/* Reflashing cross different */
		/* bootloader versions is not supported. */
		for (uBlockNum = 0; uBlockNum < lockBlockCount; ++uBlockNum) {
			uData[0] = uBlockNum & 0xff;
			uData[1] = (uBlockNum & 0xff00) >> 8;

			/* Write Block Number */
			writeRMI(ts->client,
					SynaF34Reflash_BlockNum, &uData[0], 2);

			/* Write Data Block */
			writeRMI(ts->client, SynaF34Reflash_BlockData,
					SynalockImgData, SynaFirmwareBlockSize);

			/* Move to next data block */
			SynalockImgData += SynaFirmwareBlockSize;

			/* Issue Write Lockdown Block command */
			cmd = m_uF34ReflashCmd_LockDown;
			writeRMI(ts->client, SynaF34_FlashControl,
					(unsigned char *)&cmd, 1);

			/* Wait ATTN until device is done writing the block
			 * and is ready for the next. */
			SynaWaitForATTN(1000, ts);
			CheckFlashStatus(ts, cmd);
		}

		/*
		 * Enable reflash again to finish the lockdown process.
		 * Since this lockdown process is part of the reflash process,
		 * we are enabling
		 * reflash instead, rather than resetting the device
		 * to finish the unlock procedure.
		 */
		SynaEnableFlashing(ts);
	} else
		TOUCH_E("Device already locked.\n");
}

/*
 * This function is to check the touch controller type of the touch controller
 * matches with the firmware image
 */
bool CheckTouchControllerType(struct synaptics_ts_data *ts)
{
	int ID;
	char buffer[5] = {0};
	char controllerType[20] = {0};
	unsigned char uData[4] = {0};

	/* 43 */
	readRMI(ts->client, (SynaF01QueryBase + 22),
			&SynaF01Query43Length, 1);

	if ((SynaF01Query43Length & 0x0f) > 0) {
		readRMI(ts->client, (SynaF01QueryBase + 23), &uData[0], 1);
		if (uData[0] & 0x01) {
			readRMI(ts->client, (SynaF01QueryBase + 17),
					&uData[0], 2);

			ID = ((int)uData[0] | ((int)uData[1] << 8));

			if (strnstr(controllerType, buffer, 5) != 0)
				return true;
			return false;
		} else
			return false;
	} else
		return false;
}

/*bool CheckFimrwareRevision(struct synaptics_ts_data *ts)
  {
  unsigned char uData[16];
  char revision[106] = {0};
  int i;
  readRMI(ts->client, (SynaF01QueryBase + 28 + SynaF01Query43Length),
  &uData[0], 16);

  for (i = 0; i < 0; i++)
  {
  while (uData[i] != '\0')
  {
  revision[i] = (char)uData[0];
  }
  }

  if (strcmp(revision, FW_REVISION) == 0)
  return true;
  return false;
  }*/


/* SynaScanPDT scans the Page Description Table (PDT)
 * and sets up the necessary variables
 * for the reflash process. This function is a "slim" version of the PDT scan
 * function in
 * in PDT.c, since only F34 and F01 are needed for reflash.
 */
void SynaScanPDT(struct synaptics_ts_data *ts)
{
	unsigned char address;
	unsigned char uData[2] = {0};
	unsigned char buffer[6] = {0};

	for (address = 0xe9; address > 0xc0; address = address - 6) {
		readRMI(ts->client, address, buffer, 6);

		if (!buffer[5])
			continue;
		switch (buffer[5]) {
		case 0x34:
			SynaF34DataBase = buffer[3];
			SynaF34QueryBase = buffer[0];
			break;
		case 0x01:
			SynaF01DataBase = buffer[3];
			SynaF01CommandBase = buffer[1];
			SynaF01QueryBase = buffer[0];
			break;
		}
	}

	SynaF34Reflash_BlockNum = SynaF34DataBase;
	SynaF34Reflash_BlockData = SynaF34DataBase + 1; /* +2 */
	SynaF34ReflashQuery_BootID = SynaF34QueryBase;
	SynaF34ReflashQuery_FlashPropertyQuery = SynaF34QueryBase + 1; /* +2 */
	SynaF34ReflashQuery_BlockSize = SynaF34QueryBase + 2;/*+3 */
	SynaF34ReflashQuery_FirmwareBlockCount = SynaF34QueryBase + 3; /* +5 */
	/* SynaF34ReflashQuery_ConfigBlockSize = SynaF34QueryBase + 3 */
	SynaF34_FlashControl = SynaF34DataBase + 2;
	/* no ds4 */
	SynaF34_FlashStatus = SynaF34DataBase + 3;

	/*
	SynaF34ReflashQuery_ConfigBlockCount = SynaF34QueryBase + 7
	*/
	readRMI(ts->client, SynaF34ReflashQuery_FirmwareBlockCount, buffer, 4);
	SynaFirmwareBlockCount  = buffer[0] | (buffer[1] << 8);/*no ds4 */
	SynaConfigBlockCount    = buffer[2] | (buffer[3] << 8);

	readRMI(ts->client, SynaF34ReflashQuery_BlockSize, &uData[0], 2);
	SynaConfigBlockSize = SynaFirmwareBlockSize =
		uData[0] | (uData[1] << 8);

	/* cleat ATTN */
	readRMI(ts->client, (SynaF01DataBase + 1), buffer, 1);
}

/*
 * SynaInitialize sets up the reflahs process
 */
void SynaInitialize(struct synaptics_ts_data *ts)
{
	u8 data;

	TOUCH_I("\nInitializing Reflash Process...\n");

	data = 0x00;
	writeRMI(ts->client, 0xff, &data, 1);

	/* no ds4 */
	SynaImageParser(ts);

	SynaScanPDT(ts);
}

/* SynaReadFirmwareInfo reads the F34 query registers and retrieves the block
 * size and count
 * of the firmware section of the image to be reflashed
 */
void SynaReadFirmwareInfo(struct synaptics_ts_data *ts)
{
	unsigned char uData[3] = {0};
	unsigned char product_id[11];
	int firmware_version;

	TOUCH_I("%s", __func__);


	readRMI(ts->client, SynaF01QueryBase + 11, product_id, 10);
	product_id[10] = '\0';
	TOUCH_E("Read Product ID %s\n", product_id);

	readRMI(ts->client, SynaF01QueryBase + 18, uData, 3);
	firmware_version = uData[2] << 16 | uData[1] << 8 | uData[0];
	TOUCH_E("Read Firmware Info %d\n", firmware_version);

	CheckTouchControllerType(ts);
	/* CheckFimrwareRevision(ts); */
}

/* no void SynaReadConfigInfo()
 * SynaReadBootloadID reads the F34 query registers and retrieves
 * the bootloader ID of the firmware
 */
void SynaReadBootloadID(struct synaptics_ts_data *ts)
{
	unsigned char uData[2] = {0};

	readRMI(ts->client, SynaF34ReflashQuery_BootID, &uData[0], 2);
	SynaBootloadID = uData[0] | (uData[1] << 8);
	TOUCH_E("SynaBootloadID = %d\n", SynaBootloadID);
}

/* SynaWriteBootloadID writes the bootloader ID to the F34 data register
 * to unlock the reflash process
 */
void SynaWriteBootloadID(struct synaptics_ts_data *ts)
{
	unsigned char uData[2];

	uData[0] = SynaBootloadID % 0x100;
	uData[1] = SynaBootloadID / 0x100;

	TOUCH_E("uData[0] = %x uData[0] = %x\n", uData[0], uData[1]);
	writeRMI(ts->client, SynaF34Reflash_BlockData, &uData[0], 2);
}

/* SynaEnableFlashing kicks off the reflash process
 */
void SynaEnableFlashing(struct synaptics_ts_data *ts)
{
	/*    int ret; */
	unsigned char uStatus = 0;
	enum FlashCommand cmd;
	unsigned char uData[3] = {0};
	int firmware_version;

	TOUCH_I("%s", __func__);

	TOUCH_I("\nEnable Reflash...");
	readRMI(ts->client, SynaF01DataBase, &uStatus, 1);

	if ((uStatus & 0x40) == 0) {
		/* Reflash is enabled by first reading the bootloader ID */
		/* from the firmware and write it back */
		SynaReadBootloadID(ts);
		SynaWriteBootloadID(ts);

		/* Write the "Enable Flash Programming command */
		/* to F34 Control register */
		/* Wait for ATTN and then clear the ATTN. */
		cmd = m_uF34ReflashCmd_Enable;
		writeRMI(ts->client, SynaF34_FlashControl,
				(unsigned char *)&cmd, 1);
		/* need delay after enabling flash programming*/
		udelay(1000);
		SynaWaitForATTN(1000, ts);

		/*I2C addrss may change */
		/*ConfigCommunication(); */

		/* Scan the PDT again to ensure all register offsets are correct
		 * */
		SynaScanPDT(ts);

		readRMI(ts->client, SynaF01QueryBase + 18, uData, 3);
		firmware_version = uData[2] << 16 | uData[1] << 8 | uData[0];

		/* Read the "Program Enabled" bit of the F34 Control register,
		 * */
		/* and proceed only if the */
		/* bit is set. */
		CheckFlashStatus(ts, cmd);
	}
}

/* SynaWaitForATTN waits for ATTN to be asserted within a certain time
 * threshold.
 */
unsigned int SynaWaitForATTN(int timeout, struct synaptics_ts_data *ts)
{
	unsigned char uStatus;
	/*int duration = 50; */
	/*int retry = timeout/duration; */
	/*int times = 0; */

	int trial_us = 0;
#ifdef POLLING
	do {
		uStatus = 0x00;
		readRMI(ts->client, (SynaF01DataBase + 1), &uStatus, 1);
		if (uStatus != 0)
			break;
		Sleep(duration);
		times++;
	} while (times < retry);

	if (times == retry)
		return -EPERM;
#else
	/*if (Line_WaitForAttention(timeout) == EErrorTimeout)
	  {
	  return -EPERM;
	  }*/
	while ((gpio_get_value(ts->pdata->int_pin) != 0)
			&& (trial_us < (timeout * 1000))) {
		udelay(1);
		trial_us++;
	}
	if (gpio_get_value(ts->pdata->int_pin) != 0) {
		TOUCH_E("interrupt pin is busy...");
		return -EPERM;
	}

	readRMI(ts->client, (SynaF01DataBase + 1), &uStatus, 1);
#endif
	return 0;
}
/* SynaFinalizeReflash finalizes the reflash process
 */
void SynaFinalizeReflash(struct synaptics_ts_data *ts)
{
	unsigned char uData;

	TOUCH_I("%s", __func__);

	TOUCH_I("\nFinalizing Reflash...");

	/* Issue the "Reset" command to F01 command register to reset the chip
	 * */
	/* This command will also test the new firmware image and checki */
	/* if its is valid */
	uData = 1;
	writeRMI(ts->client, SynaF01CommandBase, &uData, 1);

	/* After command reset, there will be 2 interrupt to be asserted */
	/* Simply sleep 150 ms to skip first attention */
	msleep(150);
	SynaWaitForATTN(1000, ts);

	SynaScanPDT(ts);

	readRMI(ts->client, SynaF01DataBase, &uData, 1);
}

/* SynaFlashFirmwareWrite writes the firmware section
 * of the image block by block
 */
void SynaFlashFirmwareWrite(struct synaptics_ts_data *ts)
{
	/*unsigned char *puFirmwareData = (unsigned char *)&my_image_bin[0x100];
	 * */
	unsigned char *puFirmwareData = SynafirmwareImgData;
	unsigned char uData[2];
	unsigned short blockNum;
	enum FlashCommand cmd;

	for (blockNum = 0; blockNum < SynaFirmwareBlockCount; ++blockNum) {
		if (blockNum == 0) {

			/*Block by blcok, write the block number and data */
			/*to the corresponding F34 data registers */
			uData[0] = blockNum & 0xff;
			uData[1] = (blockNum & 0xff00) >> 8;
			writeRMI(ts->client, SynaF34Reflash_BlockNum,
					&uData[0], 2);
		}

		writeRMI(ts->client, SynaF34Reflash_BlockData, puFirmwareData,
				SynaFirmwareBlockSize);
		puFirmwareData += SynaFirmwareBlockSize;

		/* Issue the "Write Firmware Block" command */
		cmd = m_uF34ReflashCmd_FirmwareWrite;
		writeRMI(ts->client, SynaF34_FlashControl,
				(unsigned char *)&cmd, 1);

		/*SynaWaitForATTN(1000,ts); */
		CheckFlashStatus(ts, cmd);
		/*TOUCH_I("[%s] blockNum=[%d], */
		/*SynaFirmwareBlockCount=[%d]\n", __func__, */
		/*blockNum, SynaFirmwareBlockCount); */
#ifdef SHOW_PROGRESS
		if (blockNum % 100 == 0)
			TOUCH_E("blk %d / %d\n",
					blockNum, SynaFirmwareBlockCount);
#endif
	}
#ifdef SHOW_PROGRESS
	TOUCH_E("blk %d / %d\n",
			SynaFirmwareBlockCount, SynaFirmwareBlockCount);
#endif
}

/* SynaFlashFirmwareWrite writes the firmware section
 * of the image block by block
 */
void SynaFlashConfigWrite(struct synaptics_ts_data *ts)
{
	/*unsigned char *puConfigData = (unsigned char *)&my_image_bin[0x100];
	 * */
	unsigned char *puConfigData = SynaconfigImgData;
	unsigned char uData[2];
	unsigned short blockNum;
	enum FlashCommand cmd;

	for (blockNum = 0; blockNum < SynaConfigBlockCount; ++blockNum)	{
		/*Block by blcok, write the block number and data */
		/*to the corresponding F34 data registers */
		uData[0] = blockNum & 0xff;
		uData[1] = (blockNum & 0xff00) >> 8;
		writeRMI(ts->client, SynaF34Reflash_BlockNum, &uData[0], 2);

		writeRMI(ts->client, SynaF34Reflash_BlockData,
				puConfigData, SynaConfigBlockSize);
		puConfigData += SynaConfigBlockSize;

		/* Issue the "Write Config Block" command */
		cmd = m_uF34ReflashCmd_ConfigWrite;
		writeRMI(ts->client, SynaF34_FlashControl,
				(unsigned char *)&cmd, 1);

		SynaWaitForATTN(100, ts);
		CheckFlashStatus(ts, cmd);
#ifdef SHOW_PROGRESS
		if (blockNum % 100 == 0)
			TOUCH_E("blk %d / %d\n",
					blockNum, SynaConfigBlockCount);
#endif
	}
#ifdef SHOW_PROGRESS
	TOUCH_E("blk %d / %d\n",
			SynaConfigBlockCount, SynaConfigBlockCount);
#endif
}

/* EraseConfigBlock erases the config block
 */
void eraseAllBlock(struct synaptics_ts_data *ts)
{
	enum FlashCommand cmd;

	/* Erase of config block is done by first entering into bootloader mode
	 * */
	SynaReadBootloadID(ts);
	SynaWriteBootloadID(ts);

	/* Command 7 to erase config block */
	cmd = m_uF34ReflashCmd_EraseAll;
	writeRMI(ts->client, SynaF34_FlashControl, (unsigned char *)&cmd, 1);

	SynaWaitForATTN(6000, ts);
	CheckFlashStatus(ts, cmd);
}

/* SynaProgramFirmware prepares the firmware writing process
 */
void SynaProgramFirmware(struct synaptics_ts_data *ts)
{
	TOUCH_E("\nProgram Firmware Section...\n");

	eraseAllBlock(ts);

	SynaFlashFirmwareWrite(ts);

	SynaFlashConfigWrite(ts);
}

/* SynaProgramFirmware prepares the firmware writing process
 */
void SynaUpdateConfig(struct synaptics_ts_data *ts)
{
	TOUCH_E("\nUpdate Config Section...\n");

	EraseConfigBlock(ts);

	SynaFlashConfigWrite(ts);
}



/* EraseConfigBlock erases the config block
 */
void EraseConfigBlock(struct synaptics_ts_data *ts)
{
	enum FlashCommand cmd;

	/* Erase of config block is done by first entering into bootloader mode
	 * */
	SynaReadBootloadID(ts);
	SynaWriteBootloadID(ts);

	/* Command 7 to erase config block */
	cmd = m_uF34ReflashCmd_EraseUIConfig;
	writeRMI(ts->client, SynaF34_FlashControl, (unsigned char *)&cmd, 1);

	SynaWaitForATTN(2000, ts);
	CheckFlashStatus(ts, cmd);
}


/* CompleteReflash reflashes the entire user image,
 * including the configuration block and firmware
 */
void CompleteReflash(struct synaptics_ts_data *ts)
{
	bool bFlashAll = true;

	SynaInitialize(ts);

	SynaReadFirmwareInfo(ts);

	SynaEnableFlashing(ts);

	SynaBootloaderLock(ts);

	if (bFlashAll)
		SynaProgramFirmware(ts);
	else
		SynaUpdateConfig(ts);

	SynaFinalizeReflash(ts);
}

#ifdef BL7
/*F34 H File */
#define MAX_PARTITION_COUNT 12
#define CDCI_MAX_BUFFER 0x200

enum EOperationSelect {
	EOperation_Firmware,
	EOperation_CoreConfig,
	EOperation_DeviceConfig,
	EOperation_CustomerSerialization,
};

static int TransCount[4][2] = {
	{EOperation_Firmware, 4},
	{EOperation_CoreConfig, 2},
	{EOperation_DeviceConfig, 1},
	{EOperation_CustomerSerialization, 1},
};

struct imagedata {
	uint8_t *data;
	uint32_t size;
};

struct F34_Query {
	unsigned char FlashKey[2];
	unsigned int BootloaderFWID;
	unsigned char MWS; /* Minium Wirte Size */
	unsigned short BlockSize;
	unsigned short FlashPageSize;
	unsigned short AdjPartitionSize;
	unsigned short FlashConfLen;
	unsigned short PayloadLen;
	unsigned char PartitionSupport[4];
	unsigned int PartitionCount;
};

static struct F34_Query g_F34Query;
static struct imagedata g_imagedata[MAX_PARTITION_COUNT];

/* -- PartitionID -> enum, size=4byte, make it 1byte */
#define Bootloader			1
#define DeviceConfig			2
#define FlashConfig			3
#define ManufacturingBlock		4
#define CustomerSerialization		5
#define GlobalParameters		6
#define CoreCode			7
#define CoreConfig			8
#define GuestCode			9
#define DisplayConfig			10
#define AFE				11

#define m_Idle				0x00
#define m_EnterBootloader		0x01
#define m_Read				0x02
#define m_Write				0x03
#define m_Erase				0x04
#define m_EraseApplication		0x05
#define m_SensorID			0x06

struct PartitionList {
	char ID;
	unsigned short BlockOffset;
	unsigned short TransferLen;
	char command;
	unsigned char Payload[2];
	int size; /*bytes */
	unsigned char *data;
} __packed;

struct ReflashInfo {
	unsigned int TransationCount;
	struct PartitionList *p_list;
};

static struct ReflashInfo info;


/*F34 C File */
#define TIMEOUT_WRITE 1000
#define TIMEOUT_ENABLE 5000
#define TIMEOUT_FINALIZE 10000
#define TIMEOUT_FIRMWARE_WRITE 2000
#define TIMEOUT_CONFIG_WRITE 2000
#define TIMEOUT_ERASE 3000
#define TIMEOUT_ERASE_CONFIG 4000
#define TIMEOUT_ERASE_GUEST 3000

const char PartitionName[MAX_PARTITION_COUNT][30] = {
	"",
	"Bootloader",
	"Device Config",
	"Flash Config",
	"Manufacturing Block",
	"Customer Serialization",
	"Global Parameters",
	"Core Code",
	"Core Config",
	"Guest Code",
	"Display Config",
	"External Touch AFE"
};
const char BL7_CommandString[7][20] = {
	"Idle",
	"Enter Bootloader",
	"Read",
	"Write",
	"Erase",
	"Erase Application",
	"Sensor ID",
};

const char OperationStatus[11][35] = {
	"Success",
	"Device Not In Bootloader Mode",/*0x01 */
	"Invalid Partition",
	"Invalid Command",
	"Invalid Block Offset",
	"Invalid Transfer",
	"Not Erased",
	"Flash Programming Key Incorrect",
	"Bad Partition Table",
	"Checksum Failed",
	"Flash Hardware Failure"
};
const char Dev_ConfigStatus[4][20] = {
	"Default",
	"Temporary",
	"Fixed",
	"Reserved"
};

struct FunctionDescriptor {
	/* note that the address is 16 bit */
	uint16_t QueryBase;
	uint16_t CommandBase;
	uint16_t ControlBase;
	uint16_t DataBase;
	uint8_t Version;
	uint8_t InterruptSourceCount;
	uint8_t ID;
};

struct Descriptor {
	uint16_t container_id;
	uint8_t container_count;
	uint32_t content_start_addr;
	uint32_t content_addr;
	uint32_t content_len;
};

static struct FunctionDescriptor g_F34Desc;
static struct FunctionDescriptor g_F01Desc;

/* Image file variables */
static uint32_t g_blcontainer_addr;
static uint8_t g_blcontainer_count;

/* F$01 variables */
static uint8_t g_F01interrupt;
static uint8_t imageFwVersion;
static bool g_bootloader_mode; /*true: in bootloader mode */


void Select_Page(struct synaptics_ts_data *ts, u16 address)
{
	u8 page = 0;
	page = (u8)(address >> 8);
	TOUCH_I("[%s] original address =%x, page = %x\n",
			__func__, address, page);
	writeRMI(ts->client, 0xff, &page, 1);
}


unsigned int WaitForATTN(int timeout, struct synaptics_ts_data *ts)
{
	unsigned char uStatus;
	/*int duration = 50; */
	/*int retry = timeout/duration; */
	/*int times = 0; */

	int trial_us = 0;
#ifdef POLLING
	do {
		uStatus = 0x00;
		Select_Page(ts, (SynaF01DataBase + 1));
		readRMI(ts->client, (SynaF01DataBase + 1), &uStatus, 1);
		if (uStatus != 0)
			break;
		Sleep(duration);
		times++;
	} while (times < retry);

	if (times == retry)
		return -EPERM;
#else
	/*if (Line_WaitForAttention(timeout) == EErrorTimeout)
	  {
	  return -EPERM;
	  }*/
	while ((gpio_get_value(ts->pdata->int_pin) != 0)
			&& (trial_us < (timeout * 1000))) {
		udelay(1);
		trial_us++;
	}
	if (gpio_get_value(ts->pdata->int_pin) != 0) {
		TOUCH_E("interrupt pin is busy...\n");
		return -EPERM;
	}
	Select_Page(ts, g_F01interrupt);
	readRMI(ts->client, (g_F01interrupt), &uStatus, 1);
#endif
	return 0;
}



/* Read the "Flash Status" of F34 data register,
 * and return true if status is 0 (no error) */
bool CheckF34Status(struct synaptics_ts_data *ts)
{
	unsigned char data[2], dev_status, o_status, BLmode, o_string;

	TOUCH_I("[%s] start", __func__);
	Select_Page(ts, g_F34Desc.DataBase);
	readRMI(ts->client, g_F34Desc.DataBase , &data[0], 2);

	o_status = 0x1f & data[0];
	dev_status = (0x60 & data[0]) >> 5; /*0x30 & data[0]; */
	BLmode = 0x80 & data[0];

	if (BLmode == 0x80)
		g_bootloader_mode = true;
	else
		g_bootloader_mode = false;

	if (o_status || BLmode) {
		if (o_status == 0x1f)
			o_string = 10;
		else
			o_string = o_status;

		if (BLmode == 0x80)
			TOUCH_I("Device in Bootloader Mode.\n");
		else
			TOUCH_I("Device in UI mode.\n");

		TOUCH_I("Device Configuration status = %s\n",
				Dev_ConfigStatus[dev_status]);
		return false;
	}

	return true;
}
bool CheckF34Status2(struct PartitionList *partition,
		struct synaptics_ts_data *ts)
{
	bool err;
	bool eReturn = true;
	unsigned char data[2], dev_status, o_status, BLmode, o_string;
	Select_Page(ts, g_F34Desc.DataBase);
	err = readRMI(ts->client, g_F34Desc.DataBase , &data[0], 2);
	if (err < 0)
		TOUCH_I("Read Fail g_F34Desc.DataBase\n");
	TOUCH_I("[%s] g_F34Desc.DataBase address = %02x\n",
			__func__, g_F34Desc.DataBase);
	TOUCH_I("[%s] g_F34Desc.DataBase data[0]=%02x, data[1]=%02x\n",
			__func__, data[0], data[1]);

	o_status = 0x1f & data[0];
	dev_status = (0x60 & data[0]) >> 5;  /*0x30 & data[0]; */
	BLmode = 0x80 & data[0];
	g_bootloader_mode = (BLmode == 0x80) ? true : false;

	if (o_status) {
		eReturn = false;
		o_string = (o_status == 0x1f) ? 10 : o_status;
		TOUCH_I(
				"Run command: %s, Partition: %s Operation status: %x (%s),\n",
				BL7_CommandString[(int)partition->command],
				PartitionName[(int)partition->ID]
				, o_status, OperationStatus[o_string]);
		TOUCH_I("Devicecfg status = %s,\n",
				Dev_ConfigStatus[dev_status]);
	}
	return eReturn;
}
bool CheckF34Command(struct synaptics_ts_data *ts)
{
	bool err;
	unsigned char data;

	TOUCH_I("[%s] Start.\n", __func__);
	Select_Page(ts, g_F34Desc.DataBase + 4);
	err = readRMI(ts->client, g_F34Desc.DataBase + 4, &data, 1);
	if (err < 0) {
		TOUCH_I(
				"[%s] Read Fail g_F34Desc.DataBase + 4 : %d",
				__func__, err);
	}

	if (data) {
		TOUCH_I(
				"Reflash Command: [%s] is not completed.\n",
				BL7_CommandString[data]);
	}
	return (data == 0) ? true : false;
}
bool CheckDevice(struct PartitionList *partition, struct synaptics_ts_data *ts)
{
	bool eReturn;
	TOUCH_I("[%s] start\n", __func__);
	eReturn = CheckF34Status2(partition, ts);
	if (!eReturn) {
		TOUCH_I("[%s] CheckF34Status = %s\n",
				__func__, eReturn ? "True" : "False");
		return eReturn;
	}
	eReturn = CheckF34Command(ts);
	if (!eReturn) {
		TOUCH_I("[%s] CheckF34Command = %s\n",
				__func__, eReturn ? "True" : "False");
		return eReturn;
	}
	return eReturn;
}
uint32_t combine_bytes(uint32_t address,
		int length, struct synaptics_ts_data *ts)
{
	uint32_t value;
	switch (length) {
	case 2:
		value = ((uint32_t)my_image_bin[address]) |
			((uint32_t)my_image_bin[address + 1] << 8);
		break;
	case 4:
		value = ((uint32_t)my_image_bin[address]) |
			((uint32_t)my_image_bin[address + 1] << 8) |
			((uint32_t)my_image_bin[address + 2] << 16) |
			((uint32_t)my_image_bin[address + 3] << 24);
		break;
	default:
		break;
	}

	return value;

}
bool ParseDescriptor(uint32_t container_addr, struct synaptics_ts_data *ts)
{
	struct Descriptor *desc;

	desc = kzalloc(sizeof(struct Descriptor), GFP_KERNEL);
	if (desc == NULL) {
		TOUCH_I("[%s] Fail to Allocate .\n", __func__);
		kfree(desc);
		desc = NULL;
		return false;
	}

	desc->container_id =
		(uint16_t)combine_bytes((container_addr + 0x04), 2, ts);
	container_addr = container_addr + 0x10;
	desc->content_len = combine_bytes((container_addr + 0x08), 4, ts);
	desc->container_count = desc->content_len / 4;
	desc->content_start_addr =
		combine_bytes((container_addr + 0x0C), 4, ts);

	switch (desc->container_id) {
	case 0x0003:
		g_blcontainer_addr = desc->content_start_addr;
		g_blcontainer_count = desc->container_count - 1;
		break;
	case 0x000E:
		g_imagedata[DeviceConfig].data =
			(uint8_t *)(my_image_bin + desc->content_start_addr);
		g_imagedata[DeviceConfig].size =
			desc->content_len;
		break;
	case 0x000F:
		g_imagedata[FlashConfig].data =
			(uint8_t *)(my_image_bin + desc->content_start_addr);
		g_imagedata[FlashConfig].size =
			desc->content_len;
		break;
	case 0x0010:
		g_imagedata[CustomerSerialization].data =
			(uint8_t *)(my_image_bin + desc->content_start_addr);
		g_imagedata[CustomerSerialization].size =
			desc->content_len;
		break;
	case 0x0011:
		g_imagedata[GlobalParameters].data =
			(uint8_t *)(my_image_bin + desc->content_start_addr);
		g_imagedata[GlobalParameters].size =
			desc->content_len;
		break;
	case 0x0012:
		g_imagedata[CoreCode].data =
			(uint8_t *)(my_image_bin + desc->content_start_addr);
		g_imagedata[CoreCode].size =
			desc->content_len;
		break;
	case 0x0013:
		g_imagedata[CoreConfig].data =
			(uint8_t *)(my_image_bin + desc->content_start_addr);
		g_imagedata[CoreConfig].size =
			desc->content_len;
		break;
	case 0x0014:
		g_imagedata[DisplayConfig].data =
			(uint8_t *)(my_image_bin + desc->content_start_addr);
		g_imagedata[DisplayConfig].size =
			desc->content_len;
		break;
	case 0x0015:
		g_imagedata[AFE].data =
			(uint8_t *)(my_image_bin + desc->content_start_addr);
		g_imagedata[AFE].size =
			desc->content_len;
		break;

	default:
		break;
	}

	kfree(desc);
	desc = NULL;

	return true;
}
bool ParseContainer(struct synaptics_ts_data *ts)
{
	bool ret = true;
	uint8_t j = 0, cont_count = 0;
	int i = 0;
	/*uint16_t container_id = 0; */
	uint32_t offset = 0;
	uint32_t top_container_addr, container_addr, cont_len;

	offset = combine_bytes(0x000C , 4, ts);
	offset += 0x10;

	cont_len = combine_bytes((offset + 0x08) , 4, ts);
	cont_count = cont_len / 4;
	/*start address */
	top_container_addr = combine_bytes((offset + 0x0C) , 4, ts);
	if (cont_count == 0) {
		TOUCH_I("[%s] cont_count is zero\n", __func__);
		ret = false;
		return ret;
	}
	/* content */
	for (i = 0; i < cont_count; i++) {
		container_addr =
			combine_bytes((top_container_addr + j) , 4, ts);
		j += 4;
		ret = ParseDescriptor(container_addr, ts);
		if (!ret) {
			TOUCH_I(
					"[%s][%d] Fail To ParseDescriptor : #%d\n",
					__func__, __LINE__, i);
			return ret;
		}
	}

	j = 4;
	top_container_addr = g_blcontainer_addr;
	for (i = 0; i < g_blcontainer_count; i++) {
		container_addr =
			combine_bytes((top_container_addr + j) , 4, ts);
		j += 4;
		ret = ParseDescriptor(container_addr, ts);
		if (!ret) {
			TOUCH_I(
					"[%s][%d] Fail To ParseDescriptor : #%d\n",
					__func__, __LINE__, i);
			return ret;
		}
	}

	return ret;
}

bool ParseImageFile(struct synaptics_ts_data *ts)
{
	bool ret = true;
	/*ReadImageFile(); */
	imageFwVersion = my_image_bin[7];

	/* Different bootloader version has different block count
	 * for the lockdown data.
	 * Need to check the bootloader version
	 * from the image file being reflashed */

	if (imageFwVersion != 0x10) {
		ret = false;
		TOUCH_I("Image file is not for bootloader 7.\n");
		return ret;
	}

	ret = ParseContainer(ts);
	if (!ret) {
		TOUCH_I("[%s] Fail to ParseContainer\n", __func__);
		return ret;
	}

	return ret;
}

/* SynaScanPDT scans the Page Description Table (PDT)
 * and sets up the necessary variables
 * for the reflash process.
 * This function scans RMI F$34 and F$01 since only these two are
 * needed for reflash.
*/
bool SynaScanPDT_BL7(struct synaptics_ts_data *ts)
{
	uint16_t address = 0;
	uint8_t buffer[6] = {0,};
	uint8_t page = 0;
	uint8_t pageAddress = 0;
	unsigned char data[22] = {0, }, offset = 0; /*, p_offset = 0; */

	g_F34Desc.ID = 0;
	g_F01Desc.ID = 0;

	for (page = 0x0; page < 2; page++) {
		writeRMI(ts->client, 0xff, &page, 1);
		for (pageAddress = 0xe9; pageAddress > 0xc0; pageAddress -= 6) {
			address = (page << 8) | pageAddress;
			readRMI(ts->client, address,
					&buffer[0], sizeof(buffer)/*6*/);

			if (buffer[5] == 0x34) {
				g_F34Desc.QueryBase =
					(page << 8) | buffer[0];
				TOUCH_I(
						"[%s]g_F34Desc.QueryBase = %x\n",
						__func__,
						g_F34Desc.QueryBase);
				g_F34Desc.CommandBase =
					(page << 8) | buffer[1];
				TOUCH_I(
						"[%s]g_F34Desc.CommandBase = %x\n",
						__func__,
						g_F34Desc.CommandBase);
				g_F34Desc.ControlBase =
					(page << 8) | buffer[2];
				TOUCH_I(
						"[%s]g_F34Desc.ControlBase = %x\n",
						__func__,
						g_F34Desc.ControlBase);
				g_F34Desc.DataBase =
					(page << 8) | buffer[3];
				TOUCH_I(
						"[%s]g_F34Desc.DataBase = %x\n",
						__func__,
						g_F34Desc.DataBase);
				g_F34Desc.Version =
					(buffer[4] >> 5) & 0x03;
				TOUCH_I(
						"[%s]g_F34Desc.Version = %x\n",
						__func__,
						g_F34Desc.Version);
				g_F34Desc.InterruptSourceCount =
					buffer[4] & 0x07;
				TOUCH_I(
						"[%s]g_F34Desc.InterruptSourceCount = %x\n",
						__func__,
						g_F34Desc.InterruptSourceCount);
				g_F34Desc.ID =
					buffer[5];
				TOUCH_I(
						"[%s]g_F34Desc.ID = %x\n",
						__func__, g_F34Desc.ID);
			} else if (buffer[5] == 0x01) {
				g_F01Desc.QueryBase
					= (page << 8) | buffer[0];
				TOUCH_I(
						"[%s]g_F01Desc.QueryBase = %x\n",
						__func__,
						g_F01Desc.QueryBase);
				g_F01Desc.CommandBase
					= (page << 8) | buffer[1];
				TOUCH_I(
						"[%s]g_F01Desc.CommandBase = %x\n",
						__func__,
						g_F01Desc.CommandBase);
				g_F01Desc.ControlBase
					= (page << 8) | buffer[2];
				TOUCH_I(
						"[%s]g_F01Desc.ControlBase = %x\n",
						__func__,
						g_F01Desc.ControlBase);
				g_F01Desc.DataBase
					= (page << 8) | buffer[3];
				TOUCH_I(
						"[%s]g_F01Desc.DataBase = %x\n",
						__func__,
						g_F01Desc.DataBase);
				g_F01Desc.Version
					= (buffer[4] >> 5) & 0x03;
				TOUCH_I(
						"[%s]g_F01Desc.Version = %x\n",
						__func__,
						g_F01Desc.Version);
				g_F01Desc.InterruptSourceCount
					= buffer[4] & 0x07;
				TOUCH_I(
						"[%s]g_F01Desc.InterruptSourceCount = %x\n",
						__func__,
						g_F01Desc.InterruptSourceCount);
				g_F01Desc.ID
					= buffer[5];
				TOUCH_I(
						"[%s]g_F01Desc.ID = %x\n",
						__func__, g_F01Desc.ID);
			} else if (buffer[5] == 0x00) {
				/* no function in this page, go to next page */
				break;
			}
		}
		Select_Page(ts, g_F01Desc.DataBase);
		readRMI(ts->client, g_F01Desc.DataBase, &buffer[0], 1);
		if ((buffer[0] & 0x40) != 0)
			break;
	}

	if (g_F34Desc.ID != 0x34) {
		TOUCH_I("F$34 is not found\n");
		return false;
	}
	if (g_F01Desc.ID != 0x01) {
		TOUCH_I("F$01 is not found\n");
		return false;
	}
	g_F01interrupt = g_F01Desc.DataBase + 1;
	/* Clear ATTN */
	Select_Page(ts, g_F01interrupt);
	readRMI(ts->client, g_F01interrupt, &buffer[0], 1);

	g_F34Query.PartitionCount = 0;

	Select_Page(ts, g_F34Desc.QueryBase + 1);
	readRMI(ts->client, g_F34Desc.QueryBase + 1, data, 22);

	g_F34Query.FlashKey[0] = data[offset];
	offset++; /*query1 */
	g_F34Query.FlashKey[1] = data[offset];
	offset++;
	g_F34Query.BootloaderFWID =
		(data[offset + 3] << 24) |
		(data[offset + 2] << 16) |
		(data[offset + 1] << 8) |
		data[offset];
	offset = offset + 4;/*query 2 */
	g_F34Query.MWS = data[offset];
	offset++;/*query3 */
	g_F34Query.BlockSize = ((data[offset + 1] << 8) | data[offset]);
	offset = offset + 2;
	g_F34Query.FlashPageSize = (data[offset + 1] << 8) | data[offset];
	offset = offset + 2;
	g_F34Query.AdjPartitionSize = (data[offset + 1] << 8) | data[offset];
	offset = offset + 2; /*query 4 */
	g_F34Query.FlashConfLen = (data[offset + 1] << 8) | data[offset];
	offset = offset + 2; /*query 5 */
	g_F34Query.PayloadLen = (data[offset + 1] << 8) | data[offset];
	offset = offset + 2; /*query 6 */

	if ((g_F34Query.BlockSize == 0) || (g_F34Query.PayloadLen == 0)) {
		TOUCH_I("Block Size/Payload Length is zero.\n");
		return false;
		/*system("pause"); */
		/*exit(EXIT_FAILURE); */
	}
	return true;
}

/* SynaInitialize sets up the reflash process
*/
bool SynaInitialize_BL7(struct synaptics_ts_data *ts)
{
	bool ret = true;
	TOUCH_I("Initializing Reflash Process...\n");
	TOUCH_I("[%s] start\n", __func__);

	/* uint8_t page = 0; */
	/*WriteRMI(0xff, &page, 1); */
	/*readRMI_BL7(0xff, &page, 1);*/ /* dummy read */

	ret = ParseImageFile(ts);
	if (!ret) {
		TOUCH_I("[%s] Fail to ParseImageFile\n", __func__);
		return ret;
	}

	ret = SynaScanPDT_BL7(ts);
	if (!ret) {
		TOUCH_I("[%s] Fail to SynaScanPDT_BL7\n", __func__);
		return ret;
	}
	return ret;
}

/* SynaReadFirmwareInfo reads the basic firmware information
*/
void SynaReadFirmwareInfo_BL7(struct synaptics_ts_data *ts)
{
	uint8_t product_id[11];
	uint8_t config_id[32];
	uint16_t i = 0;

	/* read Config ID */
	Select_Page(ts, g_F34Desc.ControlBase);
	readRMI(ts->client, g_F34Desc.ControlBase, config_id, 32);
	/*TOUCH_I("Config ID: %u\n",config_id); */
	TOUCH_I("[%s] start\n", __func__);

	for (i = 0; i < 32; i++) {
		if ((i % 8) == 0)
			TOUCH_I("0x  ");

		TOUCH_I("%02x ", config_id[i]);

		if ((i % 8) == 7)
			TOUCH_I("\n");
	}

	/* read Product ID */
	Select_Page(ts, g_F01Desc.QueryBase + 11);
	readRMI(ts->client, g_F01Desc.QueryBase + 11, product_id, 10);
	product_id[10] = '\0';
	TOUCH_I("Product ID: %s\n", product_id);

	TOUCH_I("Packrat ID: %d\n", g_F34Query.BootloaderFWID);
}

void Cleanup(struct synaptics_ts_data *ts)
{
	if (my_image_bin != NULL) {
		vfree(my_image_bin);
		my_image_bin = NULL;
	}

	if (info.p_list != NULL) {
		kfree(info.p_list);
		info.p_list = NULL;
	}
	memset(&info, 0, sizeof(struct ReflashInfo));
	memset(g_imagedata, 0, MAX_PARTITION_COUNT * sizeof(struct imagedata));
}

void Task(enum EOperationSelect operation, struct synaptics_ts_data *ts)
{
	info.TransationCount = TransCount[operation][1];
	if (info.p_list == NULL) {
		TOUCH_I("[%s] Allocate P list\n", __func__);
		info.p_list = kzalloc(4 * sizeof(struct PartitionList),
				GFP_KERNEL);
	}

	/*info.p_list = new PartitionList[info.TransationCount]; */
	TOUCH_I("[%s] start\n", __func__);
	TOUCH_I("[%s] %d\n", __func__, info.TransationCount);

	switch (operation) {
	case EOperation_Firmware:
		info.p_list[0].command =
			m_Erase; info.p_list[0].ID =
			CoreCode;
		info.p_list[1].command =
			m_Erase; info.p_list[1].ID =
			CoreConfig;
		info.p_list[2].command =
			m_Write; info.p_list[2].ID =
			CoreCode;
		info.p_list[3].command =
			m_Write; info.p_list[3].ID =
			CoreConfig;
		break;
	case EOperation_CoreConfig:
		info.p_list[0].command =
			m_Erase; info.p_list[0].ID =
			CoreConfig;
		info.p_list[1].command =
			m_Write; info.p_list[1].ID =
			CoreConfig;
		break;
	case EOperation_DeviceConfig:
		info.p_list[0].command =
			m_Write; info.p_list[0].ID =
			DeviceConfig;
		break;
	case EOperation_CustomerSerialization:
		info.p_list[0].command =
			m_Write; info.p_list[0].ID =
			CustomerSerialization;
		break;
	default:
		break;
	}
}
bool EnableFlash(struct synaptics_ts_data *ts)
{
	bool err, eReturn = true;
	struct PartitionList partition;
	partition.ID = Bootloader;
	partition.BlockOffset = 0;
	partition.TransferLen = 0;
	partition.data = NULL;
	partition.size = 0;
	partition.command = m_EnterBootloader;
	TOUCH_I("[%s] Start.\n", __func__);
	memcpy(partition.Payload, g_F34Query.FlashKey, 2);
	/* if in bootloader mode */

	/*Wait for any previous commands to complete */
	err = CheckF34Command(ts);
	if (err) {
		TOUCH_I("[%s]CheckF34Command Error : %s\n",
				__func__, err ? "True" : "False");
		Select_Page(ts, g_F34Desc.DataBase + 1);
		err = writeRMI(ts->client, g_F34Desc.DataBase + 1,
				(unsigned char *) &partition, 8);
		/*ResumeCommunication(g_F01interrupt, TIMEOUT_ENABLE); */
		WaitForATTN(TIMEOUT_ENABLE, ts);
		CheckDevice(&partition, ts);
	} else {
		eReturn = false;
		TOUCH_I("[%s] Fail to Enable Flash\n", __func__);
		return eReturn;
	}

	return eReturn;
}

bool BL7_Erase(struct PartitionList *partition, struct synaptics_ts_data *ts)
{
	bool err, eReturn = true;
	memcpy(partition->Payload, g_F34Query.FlashKey, 2);
	partition[0].BlockOffset = 0;
	partition[0].TransferLen = 0;
	TOUCH_I("[%s] start. ", __func__);

	eReturn = CheckF34Command(ts);
	if (eReturn == true) {
		Select_Page(ts, g_F34Desc.DataBase + 1);
		err = writeRMI(ts->client, g_F34Desc.DataBase + 1,
				(unsigned char *) &partition[0], 8);
	} else {
		TOUCH_I("[%s] CheckF34Command : false\n", __func__);
		return eReturn;
	}

	WaitForATTN(TIMEOUT_ERASE, ts);
	eReturn = CheckDevice(partition, ts);

	if (eReturn) {
		TOUCH_I("Erase %s successful.\n",
				PartitionName[(int)partition->ID]);
	} else {
		TOUCH_I("[%s] CheckDevice : false\n", __func__);
		return eReturn;
	}

	return eReturn;

}

bool BL7_WritePartition(struct PartitionList *partition,
		struct synaptics_ts_data *ts)
{
	bool eReturn = true;
	bool err;

	unsigned int transation_count, remain_block, cdci_count;
	unsigned int cdci_offset = 0, cdci_remain; /*, offset = 0; */
	unsigned int write_len, cdci_len; /* byte */
	unsigned int j;
	unsigned int i;
	TOUCH_I("[%s] start, Partition ID : %u\n",
			__func__, partition->ID);

	partition->command = m_Write;
	partition->BlockOffset = 0;
	partition->TransferLen = 0;

	remain_block =
		(partition->size / g_F34Query.BlockSize)
		% g_F34Query.PayloadLen;
	transation_count =
		(partition->size / g_F34Query.BlockSize)
		/ g_F34Query.PayloadLen;
	TOUCH_I(
			"[%s] remain_block = %u, partition->size = %d, g_F34Query.BlockSize = %u, g_F34Query.PayloadLen = %u\n",
			__func__,
			remain_block,
			partition->size,
			g_F34Query.BlockSize,
			g_F34Query.PayloadLen);
	TOUCH_I("[%s] transation_count = %u\n",
			__func__, transation_count);
	if (remain_block > 0)
		transation_count++;
	eReturn = CheckF34Command(ts);
	if (eReturn != true)
		return eReturn;

	/* set Partition ID and Block offset */
	Select_Page(ts, g_F34Desc.DataBase + 1);
	writeRMI(ts->client, g_F34Desc.DataBase + 1,
			(unsigned char *) &partition->ID, 1);
	writeRMI(ts->client, g_F34Desc.DataBase + 2,
			(unsigned char *) &partition->BlockOffset, 2);

	for (i = 0; i < transation_count; i++) {
		if ((i == (transation_count - 1)) && (remain_block > 0))
			partition->TransferLen = remain_block;
		else
			partition->TransferLen = g_F34Query.PayloadLen;

		/*4.	Set Transfer Length */
		writeRMI(ts->client, g_F34Desc.DataBase + 3,
				(unsigned char *) &partition->TransferLen, 2);
		/*5.	Set Command to Write */
		writeRMI(ts->client, g_F34Desc.DataBase + 4,
				(unsigned char *) &partition->command, 1);
		write_len = partition->TransferLen * g_F34Query.BlockSize;
		cdci_count = (write_len / CDCI_MAX_BUFFER);
		cdci_remain = write_len % CDCI_MAX_BUFFER;

		if (cdci_remain > 0)
			cdci_count++;
		TOUCH_I(
				"[%s] write_len = %u, cdci_count = %u, cdci_remain = %u\n",
				__func__, write_len, cdci_count, cdci_remain);
		for (j = 0; j < cdci_count; j++) {
			if ((j == (cdci_count - 1)) && (cdci_remain > 0))
				cdci_len = cdci_remain;
			else
				cdci_len = CDCI_MAX_BUFFER;
			err = writeRMI(ts->client, g_F34Desc.DataBase + 5,
					&partition->data[cdci_offset],
					cdci_len);
			TOUCH_I("[%s] cdci_len = %u\n", __func__,
					cdci_len);
			cdci_offset += cdci_len;

		}

		/* Gentle added */
		if (partition->ID == CoreCode)
			msleep(200);
		else if (partition->ID == CoreConfig)
			msleep(50);

		WaitForATTN(TIMEOUT_WRITE, ts);

		msleep(50);
		eReturn = CheckDevice(partition, ts);
	}


	if (eReturn) {
		TOUCH_I("Reflash %s successful.\n",
				PartitionName[(int)partition->ID]);
	} else {
		TOUCH_I("Reflash %s failed.\n",
				PartitionName[(int)partition->ID]);
		return eReturn;
	}

	return eReturn;

}
bool GetSourcePartitionData(struct PartitionList *partition,
		struct synaptics_ts_data *ts)
{
	partition->size = g_imagedata[(int)partition->ID].size; /*byte */
	partition->data = g_imagedata[(int)partition->ID].data;
	if (partition->size > 0) {
		return true;
	} else {
		TOUCH_I("Partition %s is not exist in image file.",
				PartitionName[(int)partition->ID]);
		return false;
	}

}

bool ReflashProcess(struct synaptics_ts_data *ts)
{
	bool eReturn = true;
	unsigned int i = 0;
	int id;
	struct PartitionList *p_list;
	TOUCH_I("[%s] ---Start Reflash----------------\n", __func__);
	if (!g_bootloader_mode) {
		eReturn = EnableFlash(ts);
		if (!eReturn) {
			TOUCH_I("[%s] Fail to EnableFlash\n", __func__);
			return eReturn;
		}
	}

	SynaScanPDT_BL7(ts);
	TOUCH_I("Bootloader Packrat ID = %d\n",
			g_F34Query.BootloaderFWID);

	for (i = 0; i < info.TransationCount; i++) {
		p_list = &info.p_list[i];

		TOUCH_I("[%s] info.p_list[i].command = %x ",
				__func__, p_list->command);
		switch ((int) p_list->command) {
		case m_Erase:
			/*if ( info.p_list[i].size > 0) */
			BL7_Erase(p_list, ts);
			break;
		case m_Write:
			id = (int) info.p_list[i].ID;
			/*if ( info.p_list[i].size > 0) */
			if (g_imagedata[id].size > 0) {
				/*Get data from image file */
				eReturn = GetSourcePartitionData(p_list, ts);
				if (!eReturn) {
					TOUCH_I(
							"Skip write partition: %s\n",
							PartitionName[id]);
					continue;
				}

				eReturn = BL7_WritePartition(p_list, ts);
				if (!eReturn) {
					TOUCH_I(
							"[%s] Fail to BL7_WritePartition\n",
							__func__);
					return eReturn;
				}
			}
			break;

		default:
			break;
		}
	}
	return eReturn;
}

bool EnableLockdown(struct synaptics_ts_data *ts)
{
	bool enable = false;
	uint8_t data;
	int i = 0;
	TOUCH_I("[%s] start\n", __func__);
	for (i = 0; i < (g_imagedata[DeviceConfig].size - 4); i++) {
		if (g_imagedata[DeviceConfig].data[i] != 0) {
			enable = true;
			TOUCH_I("[%s] DeviceConfig LockDown enabled.\n",
					__func__);
			break;
		}
	}
	if (enable) {
		Select_Page(ts, g_F34Desc.DataBase);
		readRMI(ts->client, g_F34Desc.DataBase,
				&data, sizeof(data));
		data = (data & 0x60) >> 5;
		enable = (data == 0 ? true : false);
		TOUCH_I(
				"[%s] g_F34Desc.DataBase : enable = %s, data = 0x%x\n",
				__func__, enable ? "True" : "False", data);
	}
	TOUCH_I("[%s] end : enable = %s\n",
			__func__, enable ? "True" : "False");
	return enable;
}

/* FinalizeReflash finalizes the reflash process
*/
void FinalizeReflash_BL7(struct synaptics_ts_data *ts)
{
	uint8_t data = 1;
	bool status;

	TOUCH_I("Finalizing Reflash...\n");
	Cleanup(ts);

	/* Issue the "Reset" command to F01 command register to reset the chip
	 * This command will also test the new firmware image
	 * and check if its is valid */
	Select_Page(ts, g_F01Desc.CommandBase);
	writeRMI(ts->client, g_F01Desc.CommandBase, &data, 1);

	/* After command reset, there will be 2 interrupt to be asserted */
	/* Simply sleep 150 ms to skip first attention */
	/*Sleep(8150); */
	msleep(150);

	/*ResumeCommunication(g_F01Desc.DataBase + 1, TIMEOUT_FINALIZE);
	 * */
	WaitForATTN(TIMEOUT_ERASE, ts);

	SynaScanPDT_BL7(ts);

	status = CheckF34Status(ts);
	if (status)
		TOUCH_I("Reflash Completed and Succeed.\n");
	else
		TOUCH_I("Reflash Failed.\n");
}

/*
 * CompleteReflash reflashes the entire user image,
 * including the configuration block and firmware
 */
void CompleteReflash_BL7(struct synaptics_ts_data *ts)
{
	bool do_lockdown = false;
	bool ret;
	enum EOperationSelect operation = EOperation_Firmware;

	/* parse image file and PDT */
	DO_IF(!SynaInitialize_BL7(ts), update_fail);

	/* query device information and display */
	SynaReadFirmwareInfo_BL7(ts);

	Task(operation, ts);
	CheckF34Status(ts);

	DO_IF(!ReflashProcess(ts), update_fail);

	if (g_imagedata[DeviceConfig].size > 0)
		do_lockdown = EnableLockdown(ts);
	if (do_lockdown) {
		operation = EOperation_DeviceConfig;
		Task(operation, ts);
		ret = ReflashProcess(ts);
		if (!ret) {
			TOUCH_I(
					"EOperation_DeviceConfig Reflashing Fail\n");
			goto update_fail;
		}
	}

	/* disable flash programming (exiting bootloader) */
	FinalizeReflash_BL7(ts);
	return;

update_fail:
	Cleanup(ts);
	TOUCH_I("[%s] F/W Update Fail\n", __func__);
	return;

}
#endif

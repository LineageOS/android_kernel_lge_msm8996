/*
 *  drivers/media/radio/si470x/radio-si470x-common.c
 *
 *  Driver for radios with Silicon Labs Si470x FM Radio Receivers
 *
 *  Copyright (c) 2009 Tobias Lorenz <tobias.lorenz@gmx.net>
 *  Copyright (c) 2012 Hans de Goede <hdegoede@redhat.com>
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


/*
 * History:
 * 2008-01-12	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.0
 *		- First working version
 * 2008-01-13	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.1
 *		- Improved error handling, every function now returns errno
 *		- Improved multi user access (start/mute/stop)
 *		- Channel doesn't get lost anymore after start/mute/stop
 *		- RDS support added (polling mode via interrupt EP 1)
 *		- marked default module parameters with *value*
 *		- switched from bit structs to bit masks
 *		- header file cleaned and integrated
 * 2008-01-14	Tobias Lorenz <tobias.lorenz@gmx.net>
 * 		Version 1.0.2
 * 		- hex values are now lower case
 * 		- commented USB ID for ADS/Tech moved on todo list
 * 		- blacklisted si470x in hid-quirks.c
 * 		- rds buffer handling functions integrated into *_work, *_read
 * 		- rds_command in si470x_poll exchanged against simple retval
 * 		- check for firmware version 15
 * 		- code order and prototypes still remain the same
 * 		- spacing and bottom of band codes remain the same
 * 2008-01-16	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.3
 * 		- code reordered to avoid function prototypes
 *		- switch/case defaults are now more user-friendly
 *		- unified comment style
 *		- applied all checkpatch.pl v1.12 suggestions
 *		  except the warning about the too long lines with bit comments
 *		- renamed FMRADIO to RADIO to cut line length (checkpatch.pl)
 * 2008-01-22	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.4
 *		- avoid poss. locking when doing copy_to_user which may sleep
 *		- RDS is automatically activated on read now
 *		- code cleaned of unnecessary rds_commands
 *		- USB Vendor/Product ID for ADS/Tech FM Radio Receiver verified
 *		  (thanks to Guillaume RAMOUSSE)
 * 2008-01-27	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.5
 *		- number of seek_retries changed to tune_timeout
 *		- fixed problem with incomplete tune operations by own buffers
 *		- optimization of variables and printf types
 *		- improved error logging
 * 2008-01-31	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Oliver Neukum <oliver@neukum.org>
 *		Version 1.0.6
 *		- fixed coverity checker warnings in *_usb_driver_disconnect
 *		- probe()/open() race by correct ordering in probe()
 *		- DMA coherency rules by separate allocation of all buffers
 *		- use of endianness macros
 *		- abuse of spinlock, replaced by mutex
 *		- racy handling of timer in disconnect,
 *		  replaced by delayed_work
 *		- racy interruptible_sleep_on(),
 *		  replaced with wait_event_interruptible()
 *		- handle signals in read()
 * 2008-02-08	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Oliver Neukum <oliver@neukum.org>
 *		Version 1.0.7
 *		- usb autosuspend support
 *		- unplugging fixed
 * 2008-05-07	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.8
 *		- hardware frequency seek support
 *		- afc indication
 *		- more safety checks, let si470x_get_freq return errno
 *		- vidioc behavior corrected according to v4l2 spec
 * 2008-10-20	Alexey Klimov <klimov.linux@gmail.com>
 * 		- add support for KWorld USB FM Radio FM700
 * 		- blacklisted KWorld radio in hid-core.c and hid-ids.h
 * 2008-12-03	Mark Lord <mlord@pobox.com>
 *		- add support for DealExtreme USB Radio
 * 2009-01-31	Bob Ross <pigiron@gmx.com>
 *		- correction of stereo detection/setting
 *		- correction of signal strength indicator scaling
 * 2009-01-31	Rick Bronson <rick@efn.org>
 *		Tobias Lorenz <tobias.lorenz@gmx.net>
 *		- add LED status output
 *		- get HW/SW version from scratchpad
 * 2009-06-16   Edouard Lafargue <edouard@lafargue.name>
 *		Version 1.0.10
 *		- add support for interrupt mode for RDS endpoint,
 *                instead of polling.
 *                Improves RDS reception significantly
 */

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <linux/delay.h>
#include <linux/kfifo.h>
/* kernel includes */
#include "radio-si470x.h"
#include <soc/qcom/lge/board_lge.h>


/**************************************************************************
 * Module Parameters
 **************************************************************************/

/* Spacing (kHz) */
/* 0: 200 kHz (USA, Australia) */
/* 1: 100 kHz (Europe, Japan) */
/* 2:  50 kHz */
static unsigned short space = 2;
module_param(space, ushort, 0444);
MODULE_PARM_DESC(space, "Spacing: 0=200kHz 1=100kHz *2=50kHz*");

/* De-emphasis */
/* 0: 75 us (USA) */
/* 1: 50 us (Europe, Australia, Japan) */
static unsigned short de = 1;
module_param(de, ushort, 0444);
MODULE_PARM_DESC(de, "De-emphasis: 0=75us *1=50us*");

/* Tune timeout */
static unsigned int tune_timeout = 3000;
module_param(tune_timeout, uint, 0644);
MODULE_PARM_DESC(tune_timeout, "Tune timeout: *3000*");

/* Seek timeout */
static unsigned int seek_timeout = 13000;
module_param(seek_timeout, uint, 0644);
MODULE_PARM_DESC(seek_timeout, "Seek timeout: *13000*");

static const struct v4l2_frequency_band bands[] = {
	{
		.type = V4L2_TUNER_RADIO,
		.index = 0,
		.capability = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO |
			    V4L2_TUNER_CAP_RDS | V4L2_TUNER_CAP_RDS_BLOCK_IO |
			    V4L2_TUNER_CAP_FREQ_BANDS |
			    V4L2_TUNER_CAP_HWSEEK_BOUNDED |
			    V4L2_TUNER_CAP_HWSEEK_WRAP,
		.rangelow   =  87500 * 16,
		.rangehigh  = 108000 * 16,
		.modulation = V4L2_BAND_MODULATION_FM,
	},
	{
		.type = V4L2_TUNER_RADIO,
		.index = 1,
		.capability = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO |
			    V4L2_TUNER_CAP_RDS | V4L2_TUNER_CAP_RDS_BLOCK_IO |
			    V4L2_TUNER_CAP_FREQ_BANDS |
			    V4L2_TUNER_CAP_HWSEEK_BOUNDED |
			    V4L2_TUNER_CAP_HWSEEK_WRAP,
		.rangelow   =  76000 * 16,
		.rangehigh  = 108000 * 16,
		.modulation = V4L2_BAND_MODULATION_FM,
	},
	{
		.type = V4L2_TUNER_RADIO,
		.index = 2,
		.capability = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO |
			    V4L2_TUNER_CAP_RDS | V4L2_TUNER_CAP_RDS_BLOCK_IO |
			    V4L2_TUNER_CAP_FREQ_BANDS |
			    V4L2_TUNER_CAP_HWSEEK_BOUNDED |
			    V4L2_TUNER_CAP_HWSEEK_WRAP,
		.rangelow   =  76000 * 16,
		.rangehigh  =  90000 * 16,
		.modulation = V4L2_BAND_MODULATION_FM,
	},
};

/**************************************************************************
 * Generic Functions
 **************************************************************************/

/*
 * si470x_set_band - set the band
 */
static int si470x_set_band(struct si470x_device *radio, int band)
{
	pr_info("%s enter band%d \n",__func__, band);

	if (radio->band == band)
		return 0;

	radio->band = band;
	radio->registers[SYSCONFIG2] &= ~SYSCONFIG2_BAND;
	radio->registers[SYSCONFIG2] |= radio->band << 6;
	return si470x_set_register(radio, SYSCONFIG2);
}

/*
 * si470x_set_chan - set the channel
 */
int si470x_set_chan(struct si470x_device *radio, unsigned short chan)
{
	int retval;
	bool timed_out = false;

	pr_info("%s enter\n",__func__);
	/* start tuning */
	radio->registers[CHANNEL] &= ~CHANNEL_CHAN;
	radio->registers[CHANNEL] |= CHANNEL_TUNE | chan;
	retval = si470x_set_register(radio, CHANNEL);
	if (retval < 0){
		pr_err("%s fail to channel %d\n",__func__, retval);
		goto done;
	}

	/* wait till tune operation has completed */
	reinit_completion(&radio->completion);
	retval = wait_for_completion_timeout(&radio->completion,
			msecs_to_jiffies(tune_timeout));
	if (!retval)
		timed_out = true;

	if ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0){
		pr_err("%s tune does not complete\n",__func__);
	}
	else {
		radio->seek_tune_status = TUNE_PENDING;
		si470x_q_event(radio, SILABS_EVT_TUNE_SUCC);
	}

	if (timed_out)
		pr_err("%s tune timed out after %u ms\n", __func__,tune_timeout);

	/* stop tuning */
	radio->registers[CHANNEL] &= ~CHANNEL_TUNE;
	retval = si470x_set_register(radio, CHANNEL);

done:
	return retval;
}

/*
 * si470x_get_step - get channel spacing
 */
static unsigned int si470x_get_step(struct si470x_device *radio)
{
	pr_info("%s enter\n",__func__);

	/* Spacing (kHz) */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_SPACE) >> 4) {
		/* 0: 200 kHz (USA, Australia) */
		case 0:
			return 200 * 16;
			/* 1: 100 kHz (Europe, Japan) */
		case 1:
			return 100 * 16;
			/* 2:  50 kHz */
		default:
			return 50 * 16;
	}
}


/*
 * si470x_get_freq - get the frequency
 */
static int si470x_get_freq(struct si470x_device *radio, unsigned int *freq)
{
	int chan, retval;

	pr_info("%s enter band:%d\n",__func__, radio->band);

	/* read channel */
	retval = si470x_get_register(radio, READCHAN);
	chan = radio->registers[READCHAN] & READCHAN_READCHAN;
	/* Frequency (MHz) = Spacing (kHz) x Channel + Bottom of Band (MHz) */
	*freq = chan * si470x_get_step(radio) + bands[radio->band].rangelow;
	return retval;
}

/*
 * si470x_set_freq - set the frequency
 */
int si470x_set_freq(struct si470x_device *radio, unsigned int freq)
{
	unsigned short chan;

	pr_info("%s enter, freq %d\n",__func__, freq/16);
	freq = clamp(freq, bands[radio->band].rangelow,
			bands[radio->band].rangehigh);
	/* Chan = [ Freq (Mhz) - Bottom of Band (MHz) ] / Spacing (kHz) */
	chan = (freq - bands[radio->band].rangelow) / si470x_get_step(radio);

	return si470x_set_chan(radio, chan);
}

static void update_search_list(struct si470x_device *radio, int freq)
{
	int temp_freq = freq;

	temp_freq = temp_freq -
		(radio->recv_conf.band_low_limit * TUNE_STEP_SIZE);
	temp_freq = temp_freq / 50;
	radio->srch_list.rel_freq[radio->srch_list.num_stations_found].
		rel_freq_lsb = GET_LSB(temp_freq);
	radio->srch_list.rel_freq[radio->srch_list.num_stations_found].
		rel_freq_msb = GET_MSB(temp_freq);
	radio->srch_list.num_stations_found++;
}

/*
 * si470x_set_seek - set seek
 */
static int si470x_set_seek(struct si470x_device *radio, int direction, int wrap)
{
	int retval = 0;
	bool timed_out = false;
	int i;

	pr_info("%s enter, dir %d , wrap %d\n",__func__, direction, wrap);

	/* start seeking */
	radio->registers[POWERCFG] |= POWERCFG_SEEK;
	if (wrap)
		radio->registers[POWERCFG] &= ~POWERCFG_SKMODE;
	else
		radio->registers[POWERCFG] |= POWERCFG_SKMODE;
	if (direction)
		radio->registers[POWERCFG] |= POWERCFG_SEEKUP;
	else
		radio->registers[POWERCFG] &= ~POWERCFG_SEEKUP;
	retval = si470x_set_register(radio, POWERCFG);
	if (retval < 0){
		pr_err("%s fail to write seek mode %d\n",__func__, retval);
		return retval;
	}
	/* wait till tune operation has completed */
	reinit_completion(&radio->completion);

	if((lge_get_boot_mode() == LGE_BOOT_MODE_QEM_56K)
		|| (lge_get_boot_mode() == LGE_BOOT_MODE_QEM_910K)){
		pr_info("%s fm radio AAT TEST MODE\n", __func__);
		retval = wait_for_completion_timeout(&radio->completion,
					msecs_to_jiffies(5000));
	} else {
		retval = wait_for_completion_timeout(&radio->completion,
					msecs_to_jiffies(seek_timeout));
	}

	if (!retval){
		timed_out = true;
		pr_err("%s timeout\n",__func__);

		for(i = 0; i < 16; i++ ){
			si470x_get_register(radio,i);
			pr_info("%s radio->registers[%d] : %x\n",
					__func__, i, radio->registers[i]);
		}
	}

	if ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
		pr_err("%s seek does not complete\n",__func__);

	if (radio->registers[STATUSRSSI] & STATUSRSSI_SF)
		pr_err("%s seek failed / band limit reached\n",__func__);

	/* stop seeking */
	radio->registers[POWERCFG] &= ~POWERCFG_SEEK;
	retval = si470x_set_register(radio, POWERCFG);

	/* try again, if timed out */
	if (retval == 0 && timed_out)
		return -ENODATA;

	pr_info("%s exit %d\n",__func__, retval);
	return retval;
}

void si470x_scan(struct work_struct *work)
{
	struct si470x_device *radio;
	int current_freq_khz;
	struct kfifo *data_b;
	int len = 0;
	u32 next_freq_khz;
	int retval = 0;

	pr_info("%s enter\n", __func__);

	radio = container_of(work, struct si470x_device, work_scan.work);
	radio->seek_tune_status = SEEK_PENDING;

	retval = si470x_get_freq(radio, &current_freq_khz);
	if(retval < 0){
		pr_err("%s fail to get freq\n",__func__);
		goto seek_tune_fail;
	}
	pr_info("%s current freq %d\n", __func__, current_freq_khz/16);

	while(1) {
		if (radio->is_search_cancelled == true) {
			pr_err("%s: scan cancelled\n", __func__);
			if (radio->g_search_mode == SCAN_FOR_STRONG)
				goto seek_tune_fail;
			else
				goto seek_cancelled;
			goto seek_cancelled;
		} else if (radio->mode != FM_RECV) {
			pr_err("%s: FM is not in proper state\n", __func__);
			return ;
		}

		retval = si470x_set_seek(radio, SRCH_UP, WRAP_DISABLE);
		if (retval < 0) {
			pr_err("%s seek fail %d\n", __func__, retval);
			goto seek_tune_fail;
		}

		retval = si470x_get_freq(radio, &next_freq_khz);
		if(retval < 0){
			pr_err("%s fail to get freq\n",__func__);
			goto seek_tune_fail;
		}
		pr_info("%s next freq %d\n", __func__, next_freq_khz/16);

		if (radio->registers[STATUSRSSI] & STATUSRSSI_SF){
			pr_err("%s band limit reached. Seek one more.\n",__func__);

			retval = si470x_set_seek(radio, SRCH_UP, WRAP_ENABLE);
			if (retval < 0) {
				pr_err("%s seek fail %d\n", __func__, retval);
				goto seek_tune_fail;
			}

			retval = si470x_get_freq(radio, &next_freq_khz);
			if(retval < 0){
				pr_err("%s fail to get freq\n",__func__);
				goto seek_tune_fail;
			}
			pr_info("%s next freq %d\n", __func__, next_freq_khz/16);
			si470x_q_event(radio, SILABS_EVT_TUNE_SUCC);
			break;
		}

		if (radio->g_search_mode == SCAN)
			si470x_q_event(radio, SILABS_EVT_TUNE_SUCC);

		if (radio->is_search_cancelled == true) {
			pr_err("%s: scan cancelled\n", __func__);
			if (radio->g_search_mode == SCAN_FOR_STRONG)
				goto seek_tune_fail;
			else
				goto seek_cancelled;
			goto seek_cancelled;
		} else if (radio->mode != FM_RECV) {
			pr_err("%s: FM is not in proper state\n", __func__);
			return ;
		}

		if (radio->g_search_mode == SCAN) {
			/* sleep for dwell period */
			msleep(radio->dwell_time_sec * 1000);
			/* need to queue the event when the seek completes */
			si470x_q_event(radio, SILABS_EVT_SCAN_NEXT);
		} else if (radio->g_search_mode == SCAN_FOR_STRONG) {
			update_search_list(radio, next_freq_khz);
		}

	}

seek_tune_fail:
	if (radio->g_search_mode == SCAN_FOR_STRONG) {
		len = radio->srch_list.num_stations_found * 2 +
			sizeof(radio->srch_list.num_stations_found);
		data_b = &radio->data_buf[SILABS_FM_BUF_SRCH_LIST];
		kfifo_in_locked(data_b, &radio->srch_list, len,
				&radio->buf_lock[SILABS_FM_BUF_SRCH_LIST]);
		si470x_q_event(radio, SILABS_EVT_NEW_SRCH_LIST);
	}
	pr_err("%s seek tune fail %d",__func__, retval);

seek_cancelled:
	si470x_q_event(radio, SILABS_EVT_SEEK_COMPLETE);
	radio->seek_tune_status = NO_SEEK_TUNE_PENDING;
	pr_err("%s seek cancelled %d",__func__, retval);
	return ;

}

static int si470x_vidioc_dqbuf(struct file *file, void *priv,
		struct v4l2_buffer *buffer)
{

	struct si470x_device *radio = video_get_drvdata(video_devdata(file));
	enum si470x_buf_t buf_type = -1;
	u8 buf_fifo[STD_BUF_SIZE] = {0};
	struct kfifo *data_fifo = NULL;
	u8 *buf = NULL;
	int len = 0, retval = -1;

	if ((radio == NULL) || (buffer == NULL)) {
		pr_err("%s radio/buffer is NULL\n",__func__);
		return -ENXIO;
	}

	buf_type = buffer->index;
	buf = (u8 *)buffer->m.userptr;
	len = buffer->length;
	pr_info("%s: requesting buffer %d\n", __func__, buf_type);

	if ((buf_type < SILABS_FM_BUF_MAX) && (buf_type >= 0)) {
		data_fifo = &radio->data_buf[buf_type];
		if (buf_type == SILABS_FM_BUF_EVENTS) {
			pr_info("%s before wait_event_interruptible \n", __func__);
			if (wait_event_interruptible(radio->event_queue,
						kfifo_len(data_fifo)) < 0) {
				pr_err("%s err \n", __func__);
				return -EINTR;
			}
		}
	} else {
		pr_err("%s invalid buffer type\n",__func__);
		return -EINVAL;
	}
	if (len <= STD_BUF_SIZE) {
		buffer->bytesused = kfifo_out_locked(data_fifo, &buf_fifo[0],
				len, &radio->buf_lock[buf_type]);
	} else {
		pr_err("%s kfifo_out_locked can not use len more than 128\n",__func__);
		return -EINVAL;
	}
	retval = copy_to_user(buf, &buf_fifo[0], buffer->bytesused);
	if (retval > 0) {
		pr_err("%s Failed to copy %d bytes of data\n", __func__, retval);
		return -EAGAIN;
	}
	pr_info("%s: requesting buffer exit %d\n", __func__, buf_type);
	return retval;
}

static bool check_mode(struct si470x_device *radio)
{
	bool retval = true;

	if (radio->mode == FM_OFF || radio->mode == FM_RECV)
		retval = false;

	return retval;
}

/*
 * si470x_start - switch on radio
 */
int si470x_start(struct si470x_device *radio)
{
	int retval, i;

	pr_info("%s enter\n",__func__);
	/* powercfg */
	radio->registers[POWERCFG] =
		POWERCFG_DMUTE | POWERCFG_ENABLE | POWERCFG_RDSM;
	retval = si470x_set_register(radio, POWERCFG);
	if (retval < 0){
		pr_err("%s fail to power on %d\n",__func__, retval);
		goto done;
	}
	msleep(200);

	for(i = 0; i < 16; i++ ){
		si470x_get_register(radio,i);
		pr_info("%s radio->registers[%d] : %x\n",
				__func__, i, radio->registers[i]);
	}

	/* sysconfig 1 */
	radio->registers[SYSCONFIG1] =
		(de << 11) & SYSCONFIG1_DE;		/* DE*/
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0){
		pr_err("%s fail to set DE\n", __func__);
		goto done;
	}

	/* sysconfig 2 */
	radio->registers[SYSCONFIG2] =
		(0x0 << 8)|				/* SEEKTH */
		((radio->band << 6) & SYSCONFIG2_BAND) |/* BAND */
		((space << 4) & SYSCONFIG2_SPACE) |	/* SPACE */
		SYSCONFIG2_VOLUME;					/* VOLUME (max) */
	retval = si470x_set_register(radio, SYSCONFIG2);
	if (retval < 0){
		pr_err("%s fail to set syconfig2\n", __func__);
		goto done;
	}

done:
	return retval;
}


/*
 * si470x_stop - switch off radio
 */
int si470x_stop(struct si470x_device *radio)
{
	int retval;

	pr_info("%s enter\n",__func__);

	/* sysconfig 1 */
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDS;
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
		goto done;

	/* powercfg */
	radio->registers[POWERCFG] &= ~POWERCFG_DMUTE;
	/* POWERCFG_ENABLE has to automatically go low */
	radio->registers[POWERCFG] |= POWERCFG_ENABLE |	POWERCFG_DISABLE;
	retval = si470x_set_register(radio, POWERCFG);

done:
	return retval;
}


/*
 * si470x_rds_on - switch on rds reception
 */
static int si470x_rds_on(struct si470x_device *radio)
{
	int retval;

	pr_info("%s enter\n",__func__);

	/* sysconfig 1 */
	radio->registers[SYSCONFIG1] |= SYSCONFIG1_RDS;
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
		radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDS;

	return retval;
}

static void si470x_get_rds(struct si470x_device *radio)
{
	int retval = 0;
	int i;

	mutex_lock(&radio->lock);
	retval = si470x_get_all_registers(radio);

	if (retval < 0) {
		pr_err("%s read fail%d\n",__func__, retval);
		mutex_unlock(&radio->lock);
		return;
	}
	radio->block[0] = radio->registers[RDSA];
	radio->block[1] = radio->registers[RDSB];
	radio->block[2] = radio->registers[RDSC];
	radio->block[3] = radio->registers[RDSD];

	for(i = 0; i < 4; i++)
		pr_info("%s block[%d] %x \n", __func__, i, radio->block[i]);

	radio->bler[0] = (radio->registers[STATUSRSSI] & STATUSRSSI_BLERA) >> 9;
	radio->bler[1] = (radio->registers[READCHAN] & READCHAN_BLERB) >> 14;
	radio->bler[2] = (radio->registers[READCHAN] & READCHAN_BLERC) >> 12;
	radio->bler[3] = (radio->registers[READCHAN] & READCHAN_BLERD) >> 10;
	mutex_unlock(&radio->lock);
}

static void si470x_pi_check(struct si470x_device *radio, u16 current_pi)
{
	if (radio->pi != current_pi) {
		pr_info("%s current_pi %x , radio->pi %x\n"
				, __func__, current_pi, radio->pi);
		radio->pi = current_pi;
	} else {
		pr_info("%s Received same PI code\n",__func__);
	}
}

static void si470x_pty_check(struct si470x_device *radio, u8 current_pty)
{
	if (radio->pty != current_pty) {
		pr_info("%s PTY code of radio->block[1] = %x\n", __func__, current_pty);
		radio->pty = current_pty;
	} else {
		pr_info("%s PTY repeated\n",__func__);
	}
}

static bool is_valid_freq(struct si470x_device *radio, u32 freq)
{
	u32 band_low_limit;
	u32 band_high_limit;
	u8 spacing = 0;

	band_low_limit = bands[radio->band].rangelow;
	band_high_limit = bands[radio->band].rangehigh;

	if (radio->space == 0)
		spacing = CH_SPACING_200;
	else if (radio->space == 1)
		spacing = CH_SPACING_100;
	else if (radio->space == 2)
		spacing = CH_SPACING_50;
	else
		return false;

	if ((freq >= band_low_limit) &&
			(freq <= band_high_limit) &&
			((freq - band_low_limit) % spacing == 0))
		return true;

	return false;
}

static bool is_new_freq(struct si470x_device *radio, u32 freq)
{
	u8 i = 0;

	for (i = 0; i < radio->af_info2.size; i++) {
		if (freq == radio->af_info2.af_list[i])
			return false;
	}

	return true;
}

static bool is_different_af_list(struct si470x_device *radio)
{
	u8 i = 0, j = 0;
	u32 freq;

	if (radio->af_info1.orig_freq_khz != radio->af_info2.orig_freq_khz)
		return true;

	/* freq is same, check if the AFs are same. */
	for (i = 0; i < radio->af_info1.size; i++) {
		freq = radio->af_info1.af_list[i];
		for (j = 0; j < radio->af_info2.size; j++) {
			if (freq == radio->af_info2.af_list[j])
				break;
		}

		/* freq is not there in list2 i.e list1, list2 are different.*/
		if (j == radio->af_info2.size)
			return true;
	}

	return false;
}

static void si470x_update_af_list(struct si470x_device *radio)
{

	bool retval;
	u8 i = 0;
	u8 af_data = radio->block[2] >> 8;
	u32 af_freq_khz;
	u32 tuned_freq_khz;
	struct kfifo *buff;
	struct af_list_ev ev;
	spinlock_t lock = radio->buf_lock[SILABS_FM_BUF_AF_LIST];

	si470x_get_freq(radio, &tuned_freq_khz);

	for (; i < NO_OF_AF_IN_GRP; i++, af_data = radio->block[2] & 0xFF) {

		if (af_data >= MIN_AF_CNT_CODE && af_data <= MAX_AF_CNT_CODE) {

			pr_info("%s: resetting af info, freq %u, pi %u\n",
					__func__, tuned_freq_khz, radio->pi);
			radio->af_info2.inval_freq_cnt = 0;
			radio->af_info2.cnt = 0;
			radio->af_info2.orig_freq_khz = 0;

			/* AF count. */
			radio->af_info2.cnt = af_data - NO_AF_CNT_CODE;
			radio->af_info2.orig_freq_khz = tuned_freq_khz;
			radio->af_info2.pi = radio->pi;

			pr_info("%s: current freq is %u, AF cnt is %u\n",
					__func__, tuned_freq_khz, radio->af_info2.cnt);

		} else if (af_data >= MIN_AF_FREQ_CODE &&
				af_data <= MAX_AF_FREQ_CODE &&
				radio->af_info2.orig_freq_khz != 0 &&
				radio->af_info2.size < MAX_NO_OF_AF) {

			af_freq_khz = SCALE_AF_CODE_TO_FREQ_KHZ(af_data);
			retval = is_valid_freq(radio, af_freq_khz);
			if (retval == false) {
				pr_info("%s: Invalid AF\n", __func__);
				radio->af_info2.inval_freq_cnt++;
				continue;
			}

			retval = is_new_freq(radio, af_freq_khz);
			if (retval == false) {
				pr_info("%s: Duplicate AF\n", __func__);
				radio->af_info2.inval_freq_cnt++;
				continue;
			}

			/* update the AF list */
			radio->af_info2.af_list[radio->af_info2.size++] =
				af_freq_khz;
			pr_info("%s: AF is %u\n", __func__, af_freq_khz);
			if ((radio->af_info2.size +
						radio->af_info2.inval_freq_cnt ==
						radio->af_info2.cnt) &&
					is_different_af_list(radio)) {

				/* Copy the list to af_info1. */
				radio->af_info1.cnt = radio->af_info2.cnt;
				radio->af_info1.size = radio->af_info2.size;
				radio->af_info1.pi = radio->af_info2.pi;
				radio->af_info1.orig_freq_khz =
					radio->af_info2.orig_freq_khz;
				memset(radio->af_info1.af_list,
						0,
						sizeof(radio->af_info1.af_list));

				memcpy(radio->af_info1.af_list,
						radio->af_info2.af_list,
						sizeof(radio->af_info2.af_list));

				/* AF list changed, post it to user space */
				memset(&ev, 0, sizeof(struct af_list_ev));

				ev.tune_freq_khz =
					radio->af_info1.orig_freq_khz;
				ev.pi_code = radio->pi;
				ev.af_size = radio->af_info1.size;

				memcpy(&ev.af_list[0],
						radio->af_info1.af_list,
						GET_AF_LIST_LEN(ev.af_size));

				buff = &radio->data_buf[SILABS_FM_BUF_AF_LIST];
				kfifo_in_locked(buff,
						(u8 *)&ev,
						GET_AF_EVT_LEN(ev.af_size),
						&lock);

				pr_info("%s: posting AF list evt, curr freq %u\n",
						__func__, ev.tune_freq_khz);

				si470x_q_event(radio,
						SILABS_EVT_NEW_AF_LIST);
			}
		}
	}
}

static void si470x_update_ps(struct si470x_device *radio, u8 addr, u8 ps)
{
	u8 i;
	bool ps_txt_chg = false;
	bool ps_cmplt = true;
	u8 *data;
	struct kfifo *data_b;

	pr_info("%s enter addr:%x ps:%x \n",__func__, addr, ps);

	if (radio->ps_tmp0[addr] == ps) {
		if (radio->ps_cnt[addr] < PS_VALIDATE_LIMIT) {
			radio->ps_cnt[addr]++;
		} else {
			radio->ps_cnt[addr] = PS_VALIDATE_LIMIT;
			radio->ps_tmp1[addr] = ps;
		}
	} else if (radio->ps_tmp1[addr] == ps) {
		if (radio->ps_cnt[addr] >= PS_VALIDATE_LIMIT) {
			ps_txt_chg = true;
			radio->ps_cnt[addr] = PS_VALIDATE_LIMIT + 1;
		} else {
			radio->ps_cnt[addr] = PS_VALIDATE_LIMIT;
		}
		radio->ps_tmp1[addr] = radio->ps_tmp0[addr];
		radio->ps_tmp0[addr] = ps;
	} else if (!radio->ps_cnt[addr]) {
		radio->ps_tmp0[addr] = ps;
		radio->ps_cnt[addr] = 1;
	} else {
		radio->ps_tmp1[addr] = ps;
	}

	if (ps_txt_chg) {
		for (i = 0; i < MAX_PS_LEN; i++) {
			if (radio->ps_cnt[i] > 1)
				radio->ps_cnt[i]--;
		}
	}

	for (i = 0; i < MAX_PS_LEN; i++) {
		if (radio->ps_cnt[i] < PS_VALIDATE_LIMIT) {
			pr_info("%s ps_cnt[%d] %d\n",__func__, i ,radio->ps_cnt[i]);
			ps_cmplt = false;
			return;
		}
	}

	if (ps_cmplt) {
		for (i = 0; (i < MAX_PS_LEN) &&
				(radio->ps_display[i] == radio->ps_tmp0[i]); i++)
			;
		if (i == MAX_PS_LEN) {
			pr_info("%s Same PS string repeated\n",__func__);
			return;
		}

		for (i = 0; i < MAX_PS_LEN; i++)
			radio->ps_display[i] = radio->ps_tmp0[i];

		data = kmalloc(PS_EVT_DATA_LEN, GFP_ATOMIC);
		if (data != NULL) {
			data[0] = NO_OF_PS;
			data[1] = radio->pty;
			data[2] = (radio->pi >> 8) & 0xFF;
			data[3] = (radio->pi & 0xFF);
			data[4] = 0;
			memcpy(data + OFFSET_OF_PS,
					radio->ps_tmp0, MAX_PS_LEN);
			data_b = &radio->data_buf[SILABS_FM_BUF_PS_RDS];
			kfifo_in_locked(data_b, data, PS_EVT_DATA_LEN,
					&radio->buf_lock[SILABS_FM_BUF_PS_RDS]);
			pr_info("%s Q the PS event\n", __func__);
			si470x_q_event(radio, SILABS_EVT_NEW_PS_RDS);
			kfree(data);
		} else {
			pr_err("%s Memory allocation failed for PTY\n", __func__);
		}
	}
}

static void display_rt(struct si470x_device *radio)
{
	u8 len = 0, i = 0;
	u8 *data;
	struct kfifo *data_b;
	bool rt_cmplt = true;

	pr_info("%s enter\n",__func__);

	for (i = 0; i < MAX_RT_LEN; i++) {
		if (radio->rt_cnt[i] < RT_VALIDATE_LIMIT) {
			pr_info("%s rt_cnt %d\n", __func__, radio->rt_cnt[i]);
			rt_cmplt = false;
			return;
		}
		if (radio->rt_tmp0[i] == END_OF_RT)
			break;
	}
	if (rt_cmplt) {
		while ((len < MAX_RT_LEN) && (radio->rt_tmp0[len] != END_OF_RT))
			len++;

		for (i = 0; (i < len) &&
				(radio->rt_display[i] == radio->rt_tmp0[i]); i++);
		if (i == len) {
			pr_info("%s Same RT string repeated\n",__func__);
			return;
		}
		for (i = 0; i < len; i++)
			radio->rt_display[i] = radio->rt_tmp0[i];
		data = kmalloc(len + OFFSET_OF_RT, GFP_ATOMIC);
		if (data != NULL) {
			data[0] = len; /* len of RT */
			data[1] = radio->pty;
			data[2] = (radio->pi >> 8) & 0xFF;
			data[3] = (radio->pi & 0xFF);
			data[4] = radio->rt_flag;
			memcpy(data + OFFSET_OF_RT, radio->rt_display, len);
			data_b = &radio->data_buf[SILABS_FM_BUF_RT_RDS];
			kfifo_in_locked(data_b, data, OFFSET_OF_RT + len,
					&radio->buf_lock[SILABS_FM_BUF_RT_RDS]);
			pr_info("%s Q the RT event\n", __func__);
			si470x_q_event(radio, SILABS_EVT_NEW_RT_RDS);
			kfree(data);
		} else {
			pr_err("%s Memory allocation failed for PTY\n", __func__);
		}
	}
}

static void rt_handler(struct si470x_device *radio, u8 ab_flg,
		u8 cnt, u8 addr, u8 *rt)
{
	u8 i, errcnt, blermax;
	bool rt_txt_chg = 0;

	pr_info("%s enter\n",__func__);

	if (ab_flg != radio->rt_flag && radio->valid_rt_flg) {
		for (i = 0; i < sizeof(radio->rt_cnt); i++) {
			if (!radio->rt_tmp0[i]) {
				radio->rt_tmp0[i] = ' ';
				radio->rt_cnt[i]++;
			}
		}
		memset(radio->rt_cnt, 0, sizeof(radio->rt_cnt));
		memset(radio->rt_tmp0, 0, sizeof(radio->rt_tmp0));
		memset(radio->rt_tmp1, 0, sizeof(radio->rt_tmp1));
	}

	radio->rt_flag = ab_flg;
	radio->valid_rt_flg = true;

	for (i = 0; i < cnt; i++) {
		if((i < 2) && (cnt > 2)){
			errcnt = radio->bler[2];
			blermax = CORRECTED_THREE_TO_FIVE;
		} else {
			errcnt = radio->bler[3];
			blermax = CORRECTED_THREE_TO_FIVE;
		}
		if(errcnt <= blermax) {
			if(!rt[i])
				rt[i] = ' ';
			if (radio->rt_tmp0[addr+i] == rt[i]) {
				if (radio->rt_cnt[addr+i] < RT_VALIDATE_LIMIT) {
					radio->rt_cnt[addr+i]++;
				} else {
					radio->rt_cnt[addr+i] = RT_VALIDATE_LIMIT;
					radio->rt_tmp1[addr+i] = rt[i];
				}
			} else if (radio->rt_tmp1[addr+i] == rt[i]) {
				if (radio->rt_cnt[addr+i] >= RT_VALIDATE_LIMIT) {
					rt_txt_chg = true;
					radio->rt_cnt[addr+i] = RT_VALIDATE_LIMIT + 1;
				} else {
					radio->rt_cnt[addr+i] = RT_VALIDATE_LIMIT;
				}
				radio->rt_tmp1[addr+i] = radio->rt_tmp0[addr+i];
				radio->rt_tmp0[addr+i] = rt[i];
			} else if (!radio->rt_cnt[addr+i]) {
				radio->rt_tmp0[addr+i] = rt[i];
				radio->rt_cnt[addr+i] = 1;
			} else {
				radio->rt_tmp1[addr+i] = rt[i];
			}
		}
	}

	if (rt_txt_chg) {
		for (i = 0; i < MAX_RT_LEN; i++) {
			if (radio->rt_cnt[i] > 1)
				radio->rt_cnt[i]--;
		}
	}
	display_rt(radio);
}

static void si470x_raw_rds(struct si470x_device *radio)
{
	u16 aid, app_grp_typ;

	aid = radio->block[3];
	app_grp_typ = radio->block[1] & APP_GRP_typ_MASK;
	pr_info("%s app_grp_typ = %x\n", __func__, app_grp_typ);
	pr_info("%s AID = %x", __func__, aid);

	switch (aid) {
		case ERT_AID:
			radio->utf_8_flag = (radio->block[2] & 1);
			radio->formatting_dir = EXTRACT_BIT(radio->block[2],
					ERT_FORMAT_DIR_BIT);
			if (radio->ert_carrier != app_grp_typ) {
				si470x_q_event(radio, SILABS_EVT_NEW_ODA);
				radio->ert_carrier = app_grp_typ;
			}
			break;
		case RT_PLUS_AID:
			/*Extract 5th bit of MSB (b7b6b5b4b3b2b1b0)*/
			radio->rt_ert_flag = EXTRACT_BIT(radio->block[2],
					RT_ERT_FLAG_BIT);
			if (radio->rt_plus_carrier != app_grp_typ) {
				si470x_q_event(radio, SILABS_EVT_NEW_ODA);
				radio->rt_plus_carrier = app_grp_typ;
			}
			break;
		default:
			pr_info("%s Not handling the AID of %x\n", __func__, aid);
			break;
	}
}

static void si470x_ev_ert(struct si470x_device *radio)
{
	u8 *data = NULL;
	struct kfifo *data_b;

	if (radio->ert_len <= 0)
		return;

	pr_info("%s enter\n", __func__);
	data = kmalloc((radio->ert_len + ERT_OFFSET), GFP_ATOMIC);
	if (data != NULL) {
		data[0] = radio->ert_len;
		data[1] = radio->utf_8_flag;
		data[2] = radio->formatting_dir;
		memcpy((data + ERT_OFFSET), radio->ert_buf, radio->ert_len);
		data_b = &radio->data_buf[SILABS_FM_BUF_ERT];
		kfifo_in_locked(data_b, data, (radio->ert_len + ERT_OFFSET),
				&radio->buf_lock[SILABS_FM_BUF_ERT]);
		si470x_q_event(radio, SILABS_EVT_NEW_ERT);
		kfree(data);
	}
}

static void si470x_buff_ert(struct si470x_device *radio)
{
	int i;
	u16 info_byte = 0;
	u8 byte_pair_index;

	byte_pair_index = radio->block[1] & APP_GRP_typ_MASK;
	if (byte_pair_index == 0) {
		radio->c_byt_pair_index = 0;
		radio->ert_len = 0;
	}
	if (radio->c_byt_pair_index == byte_pair_index) {
		for (i = 2; i <= 3; i++) {
			info_byte = radio->block[i];
			pr_info("%s info_byte = %x\n", __func__, info_byte);
			pr_info("%s ert_len = %x\n", __func__, radio->ert_len);
			if (radio->ert_len > (MAX_ERT_LEN - 2))
				return;
			radio->ert_buf[radio->ert_len] = radio->block[i] >> 8;
			radio->ert_buf[radio->ert_len + 1] =
				radio->block[i] & 0xFF;
			radio->ert_len += ERT_CNT_PER_BLK;
			pr_info("%s utf_8_flag = %d\n", __func__, radio->utf_8_flag);
			if ((radio->utf_8_flag == 0) &&
					(info_byte == END_OF_RT)) {
				radio->ert_len -= ERT_CNT_PER_BLK;
				break;
			} else if ((radio->utf_8_flag == 1) &&
					(radio->block[i] >> 8 == END_OF_RT)) {
				info_byte = END_OF_RT;
				radio->ert_len -= ERT_CNT_PER_BLK;
				break;
			} else if ((radio->utf_8_flag == 1) &&
					((radio->block[i] & 0xFF)
					 == END_OF_RT)) {
				info_byte = END_OF_RT;
				radio->ert_len--;
				break;
			}
		}
		if ((byte_pair_index == MAX_ERT_SEGMENT) ||
				(info_byte == END_OF_RT)) {
			si470x_ev_ert(radio);
			radio->c_byt_pair_index = 0;
			radio->ert_len = 0;
		}
		radio->c_byt_pair_index++;
	} else {
		radio->ert_len = 0;
		radio->c_byt_pair_index = 0;
	}
}

static void si470x_rt_plus(struct si470x_device *radio)
{
	u8 tag_type1, tag_type2;
	u8 *data = NULL;
	int len = 0;
	u16 grp_typ;
	struct kfifo *data_b;

	grp_typ = radio->block[1] & APP_GRP_typ_MASK;
	/*
	 *right most 3 bits of Lsb of block 2
	 * and left most 3 bits of Msb of block 3
	 */
	tag_type1 = (((grp_typ & TAG1_MSB_MASK) << TAG1_MSB_OFFSET) |
			(radio->block[2] >> TAG1_LSB_OFFSET));
	/*
	 *right most 1 bit of lsb of 3rd block
	 * and left most 5 bits of Msb of 4th block
	 */
	tag_type2 = (((radio->block[2] & TAG2_MSB_MASK)
				<< TAG2_MSB_OFFSET) |
			(radio->block[2] >> TAG2_LSB_OFFSET));

	if (tag_type1 != DUMMY_CLASS)
		len += RT_PLUS_LEN_1_TAG;
	if (tag_type2 != DUMMY_CLASS)
		len += RT_PLUS_LEN_1_TAG;

	if (len != 0) {
		len += RT_PLUS_OFFSET;
		data = kmalloc(len, GFP_ATOMIC);
	} else {
		pr_err("%s:Len is zero\n", __func__);
		return;
	}
	if (data != NULL) {
		data[0] = len;
		len = RT_ERT_FLAG_OFFSET;
		data[len++] = radio->rt_ert_flag;
		if (tag_type1 != DUMMY_CLASS) {
			data[len++] = tag_type1;
			/*
			 *start position of tag1
			 *right most 5 bits of msb of 3rd block
			 *and left most bit of lsb of 3rd block
			 */
			data[len++] = (radio->block[2] >> TAG1_POS_LSB_OFFSET)
				& TAG1_POS_MSB_MASK;
			/*
			 *length of tag1
			 *left most 6 bits of lsb of 3rd block
			 */
			data[len++] = (radio->block[2] >> TAG1_LEN_OFFSET) &
				TAG1_LEN_MASK;
		}
		if (tag_type2 != DUMMY_CLASS) {
			data[len++] = tag_type2;
			/*
			 *start position of tag2
			 *right most 3 bit of msb of 4th block
			 *and left most 3 bits of lsb of 4th block
			 */
			data[len++] = (radio->block[3] >> TAG2_POS_LSB_OFFSET) &
				TAG2_POS_MSB_MASK;
			/*
			 *length of tag2
			 *right most 5 bits of lsb of 4th block
			 */
			data[len++] = radio->block[3] & TAG2_LEN_MASK;
		}
		data_b = &radio->data_buf[SILABS_FM_BUF_RT_PLUS];
		kfifo_in_locked(data_b, data, len,
				&radio->buf_lock[SILABS_FM_BUF_RT_PLUS]);
		si470x_q_event(radio, SILABS_EVT_NEW_RT_PLUS);
		kfree(data);
	} else {
		pr_err("%s:memory allocation failed\n", __func__);
	}
}

void si470x_rds_handler(struct work_struct *worker)
{
	struct si470x_device *radio;
	u8 rt_blks[NO_OF_RDS_BLKS];
	u8 grp_type, addr, ab_flg;
	int i = 0;

	radio = container_of(worker, struct si470x_device, rds_worker);

	if (!radio) {
		pr_err("%s:radio is null\n", __func__);
		return;
	}

	pr_info("%s enter\n", __func__);

	si470x_get_rds(radio);

	if(radio->bler[0] < CORRECTED_THREE_TO_FIVE)
		si470x_pi_check(radio, radio->block[0]);

	if(radio->bler[1] < CORRECTED_ONE_TO_TWO){
		grp_type = radio->block[1] >> OFFSET_OF_GRP_TYP;
		pr_info("%s grp_type = %d\n", __func__, grp_type);
	} else {
		pr_err("%s invalid data\n",__func__);

		for(i = 0; i < 16; i++ ){
			si470x_get_register(radio,i);
			pr_info("%s radio->registers[%d] : %x\n",
					__func__, i, radio->registers[i]);
		}
		return;
	}
	if (grp_type & 0x01)
		si470x_pi_check(radio, radio->block[2]);

	si470x_pty_check(radio, (radio->block[1] >> OFFSET_OF_PTY) & PTY_MASK);

	switch (grp_type) {
		case RDS_TYPE_0A:
			if(radio->bler[2] <= CORRECTED_THREE_TO_FIVE)
				si470x_update_af_list(radio);
			/*  fall through */
		case RDS_TYPE_0B:
			addr = (radio->block[1] & PS_MASK) * NO_OF_CHARS_IN_EACH_ADD;
			pr_info("%s RDS is PS\n", __func__);
			if(radio->bler[3] <= CORRECTED_THREE_TO_FIVE){
				si470x_update_ps(radio, addr+0, radio->block[3] >> 8);
				si470x_update_ps(radio, addr+1, radio->block[3] & 0xff);
			}
			break;
		case RDS_TYPE_2A:
			pr_info("%s RDS is RT 2A group\n", __func__);
			rt_blks[0] = (u8)(radio->block[2] >> 8);
			rt_blks[1] = (u8)(radio->block[2] & 0xFF);
			rt_blks[2] = (u8)(radio->block[3] >> 8);
			rt_blks[3] = (u8)(radio->block[3] & 0xFF);
			addr = (radio->block[1] & 0xf) * 4;
			ab_flg = (radio->block[1] & 0x0010) >> 4;
			rt_handler(radio, ab_flg, CNT_FOR_2A_GRP_RT, addr, rt_blks);
			break;
		case RDS_TYPE_2B:
			pr_info("%s RDS is RT 2B group\n",__func__);
			rt_blks[0] = (u8)(radio->block[3] >> 8);
			rt_blks[1] = (u8)(radio->block[3] & 0xFF);
			rt_blks[2] = 0;
			rt_blks[3] = 0;
			addr = (radio->block[1] & 0xf) * 2;
			ab_flg = (radio->block[1] & 0x0010) >> 4;
			radio->rt_tmp0[MAX_LEN_2B_GRP_RT] = END_OF_RT;
			radio->rt_tmp1[MAX_LEN_2B_GRP_RT] = END_OF_RT;
			radio->rt_cnt[MAX_LEN_2B_GRP_RT] = RT_VALIDATE_LIMIT;
			rt_handler(radio, ab_flg, CNT_FOR_2B_GRP_RT, addr, rt_blks);
			break;
		case RDS_TYPE_3A:
			pr_info("%s RDS is 3A group\n",__func__);
			si470x_raw_rds(radio);
			break;
		default:
			pr_err("%s Not handling the group type %d\n", __func__, grp_type);
			break;
	}
	pr_info("%s rt_plus_carrier = %x\n", __func__, radio->rt_plus_carrier);
	pr_info("%s ert_carrier = %x\n", __func__, radio->ert_carrier);
	if (radio->rt_plus_carrier && (grp_type == radio->rt_plus_carrier))
		si470x_rt_plus(radio);
	else if (radio->ert_carrier && (grp_type == radio->ert_carrier))
		si470x_buff_ert(radio);
	return;
}

/*
 * si470x_fops_read - read RDS data
 */
static ssize_t si470x_fops_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	unsigned int block_count = 0;

	pr_info("%s enter\n",__func__);

	/* switch on rds reception */
	if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
		si470x_rds_on(radio);

	/* block if no new data available */
	while (radio->wr_index == radio->rd_index) {
		if (file->f_flags & O_NONBLOCK) {
			retval = -EWOULDBLOCK;
			goto done;
		}
		if (wait_event_interruptible(radio->read_queue,
					radio->wr_index != radio->rd_index) < 0) {
			retval = -EINTR;
			goto done;
		}
	}

	/* calculate block count from byte count */
	count /= 3;

	/* copy RDS block out of internal buffer and to user buffer */
	while (block_count < count) {
		if (radio->rd_index == radio->wr_index)
			break;

		/* always transfer rds complete blocks */
		if (copy_to_user(buf, &radio->buffer[radio->rd_index], 3))
			/* retval = -EFAULT; */
			break;

		/* increment and wrap read pointer */
		radio->rd_index += 3;
		if (radio->rd_index >= radio->buf_size)
			radio->rd_index = 0;

		/* increment counters */
		block_count++;
		buf += 3;
		retval += 3;
	}

done:
	return retval;
}


/*
 * si470x_fops_poll - poll RDS data
 */
static unsigned int si470x_fops_poll(struct file *file,
		struct poll_table_struct *pts)
{
	struct si470x_device *radio = video_drvdata(file);
	unsigned long req_events = poll_requested_events(pts);
	int retval = v4l2_ctrl_poll(file, pts);

	pr_info("%s enter\n",__func__);

	if (req_events & (POLLIN | POLLRDNORM)) {
		/* switch on rds reception */
		if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
			si470x_rds_on(radio);

		poll_wait(file, &radio->read_queue, pts);

		if (radio->rd_index != radio->wr_index)
			retval |= POLLIN | POLLRDNORM;
	}

	return retval;
}

/**************************************************************************
 * Video4Linux Interface
 **************************************************************************/
void si470x_q_event(struct si470x_device *radio,
		enum si470x_evt_t event)
{

	struct kfifo *data_b;
	unsigned char evt = event;

	data_b = &radio->data_buf[SILABS_FM_BUF_EVENTS];

	pr_info("%s updating event_q with event %x\n", __func__, event);
	if (kfifo_in_locked(data_b,
				&evt,
				1,
				&radio->buf_lock[SILABS_FM_BUF_EVENTS]))
		wake_up_interruptible(&radio->event_queue);
}

static int si470x_g_ctrl(struct file *file, void *priv, struct v4l2_control *ctrl)
{

	//	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	pr_info("%s enter, id: %x value: %d\n",__func__, ctrl->id,ctrl->value);

	switch (ctrl->id) {
		case V4L2_CID_PRIVATE_SILABS_RDSGROUP_PROC:
			break;
		case V4L2_CID_AUDIO_VOLUME:
			break;
		case V4L2_CID_AUDIO_MUTE:
			break;
		default:
			return -EINVAL;
	}

	return retval;
}

static int si470x_enable(struct si470x_device *radio)
{
	int retval = 0;

	mutex_lock(&radio->lock);

	retval = si470x_get_register(radio, POWERCFG);
	if((radio->registers[POWERCFG] & POWERCFG_ENABLE)== 0){
		si470x_start(radio);
	} else {
		pr_info("%s already turn on\n",__func__);
	}

	if((radio->registers[SYSCONFIG1] &  SYSCONFIG1_STCIEN) == 0){
		radio->registers[SYSCONFIG1] |= SYSCONFIG1_RDSIEN;
		radio->registers[SYSCONFIG1] |= SYSCONFIG1_STCIEN;
		radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_GPIO2;
		radio->registers[SYSCONFIG1] |= 0x1 << 2;
		retval = si470x_set_register(radio, SYSCONFIG1);
		if (retval < 0) {
			pr_err("%s set register fail\n",__func__);
			goto done;
		} else{
			si470x_q_event(radio, SILABS_EVT_RADIO_READY);
			radio->mode = FM_RECV;
		}
	} else {
		si470x_q_event(radio, SILABS_EVT_RADIO_READY);
		radio->mode = FM_RECV;
	}
done:
	mutex_unlock(&radio->lock);
	return retval;

}

static int si470x_disable(struct si470x_device *radio)
{
	int retval = 0;

	/* disable RDS/STC interrupt */
	radio->registers[SYSCONFIG1] |= ~SYSCONFIG1_RDSIEN;
	radio->registers[SYSCONFIG1] |= ~SYSCONFIG1_STCIEN;
	retval = si470x_set_register(radio, SYSCONFIG1);
	if(retval < 0){
		pr_err("%s fail to disable RDS/SCT interrupt\n",__func__);
		goto done;
	}
	retval = si470x_stop(radio);
	if(retval < 0){
		pr_err("%s fail to turn off fmradio\n",__func__);
		goto done;
	}

	if (radio->mode == FM_TURNING_OFF || radio->mode == FM_RECV) {
		pr_info("%s: posting SILABS_EVT_RADIO_DISABLED event\n",
				__func__);
		si470x_q_event(radio, SILABS_EVT_RADIO_DISABLED);
		radio->mode = FM_OFF;
	}
	flush_workqueue(radio->wqueue);

done:
	return retval;
}

bool is_valid_srch_mode(int srch_mode)
{
	if ((srch_mode >= SI470X_MIN_SRCH_MODE) &&
			(srch_mode <= SI470X_MAX_SRCH_MODE))
		return 1;
	else
		return 0;
}

static int si470x_s_ctrl(struct file *file, void *priv, struct v4l2_control *ctrl)
{

	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	unsigned short de_s;
	int space_s,snr;

	pr_info("%s enter, ctrl->id: %x, value:%d \n", __func__, ctrl->id, ctrl->value);

	switch (ctrl->id) {
		case V4L2_CID_PRIVATE_SILABS_STATE:
			if (ctrl->value == FM_RECV) {
				if (check_mode(radio) != 0) {
					pr_err("%s: fm is not in proper state\n",
							__func__);
					retval = -EINVAL;
					goto end;
				}
				radio->mode = FM_RECV_TURNING_ON;

				retval = si470x_enable(radio);
				if (retval < 0) {
					pr_err("%s Error while enabling RECV FM %d\n",
							__func__, retval);
					radio->mode = FM_OFF;
					goto end;
				}
			} else if (ctrl->value == FM_OFF) {
				radio->mode = FM_TURNING_OFF;
				retval = si470x_disable(radio);
				if (retval < 0) {
					pr_err("Err on disable recv FM %d\n", retval);
					radio->mode = FM_RECV;
					goto end;
				}
			}
			break;
		case V4L2_CID_AUDIO_VOLUME:
			radio->registers[SYSCONFIG2] &= ~SYSCONFIG2_VOLUME;
			radio->registers[SYSCONFIG2] |= ctrl->value;
			retval = si470x_set_register(radio, SYSCONFIG2);
			break;
		case V4L2_CID_AUDIO_MUTE:
			if (ctrl->value)
				radio->registers[POWERCFG] &= ~POWERCFG_DMUTE;
			else
				radio->registers[POWERCFG] |= POWERCFG_DMUTE;
			retval = si470x_set_register(radio, POWERCFG);
			break;
		case V4L2_CID_PRIVATE_SILABS_EMPHASIS:
			pr_info("%s value : %d\n", __func__, ctrl->value);
			de_s = (u16)ctrl->value;
			radio->registers[SYSCONFIG1] =
				( de_s << 11) & SYSCONFIG1_DE;
			radio->registers[SYSCONFIG1] |= SYSCONFIG1_RDSIEN;
			radio->registers[SYSCONFIG1] |= SYSCONFIG1_STCIEN;
			radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_GPIO2;
			radio->registers[SYSCONFIG1] |= 0x1 << 2;
			retval = si470x_set_register(radio, SYSCONFIG1);
			break;
		case V4L2_CID_PRIVATE_SILABS_RDS_STD:
			retval = 0;
			break;
		case V4L2_CID_PRIVATE_SILABS_SPACING:
			space_s = ctrl->value;
			radio->space = ctrl->value;
			radio->registers[SYSCONFIG2] =
				(0x0 << 8)|				/* SEEKTH */
				((radio->band << 6) & SYSCONFIG2_BAND) |/* BAND */
				((space_s << 4) & SYSCONFIG2_SPACE) |	/* SPACE */
				SYSCONFIG2_VOLUME;					/* VOLUME (max) */
			retval = si470x_set_register(radio, SYSCONFIG2);
			break;
		case V4L2_CID_PRIVATE_SILABS_SRCHON:
			si470x_search(radio, (bool)ctrl->value);
			break;
		case V4L2_CID_PRIVATE_SILABS_SET_AUDIO_PATH:
		case V4L2_CID_PRIVATE_SILABS_SRCH_ALGORITHM:
		case V4L2_CID_PRIVATE_SILABS_REGION:
			retval = 0;
			break;
		case V4L2_CID_PRIVATE_SILABS_RDSGROUP_MASK:
		case V4L2_CID_PRIVATE_SILABS_RDSGROUP_PROC:
			if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
				si470x_rds_on(radio);
			retval = 0;
			break;
		case V4L2_CID_PRIVATE_SILABS_AF_JUMP:
		case V4L2_CID_PRIVATE_SILABS_SRCH_CNT:
			retval = 0;
			break;
		case V4L2_CID_PRIVATE_SILABS_SINR_THRESHOLD:
			snr = ctrl->value;
			radio->registers[SYSCONFIG3] &= ~SYSCONFIG3_SKSNR;
			if(snr >= 0 && snr < 16)
				radio->registers[SYSCONFIG3] |= snr << 4;
			else
				radio->registers[SYSCONFIG3] |= 3 << 4;
			retval = si470x_set_register(radio, SYSCONFIG3);
			if(retval < 0)
				pr_err("%s fail to write snr\n",__func__);
			si470x_get_register(radio, SYSCONFIG3);
			pr_info("%s SYSCONFIG3:%x\n", __func__, radio->registers[SYSCONFIG3]);
			break;
		case V4L2_CID_PRIVATE_SILABS_LP_MODE:
		case V4L2_CID_PRIVATE_SILABS_ANTENNA:
		case V4L2_CID_PRIVATE_SILABS_RDSON:
			break;
		case V4L2_CID_PRIVATE_SILABS_SRCHMODE:
			if (is_valid_srch_mode(ctrl->value)) {
				radio->g_search_mode = ctrl->value;
			} else {
				pr_err("%s: srch mode is not valid\n", __func__);
				retval = -EINVAL;
				goto end;
			}
			break;
		case V4L2_CID_PRIVATE_SILABS_PSALL:
			break;
		case V4L2_CID_PRIVATE_SILABS_RXREPEATCOUNT:
			break;
		case V4L2_CID_PRIVATE_SILABS_SCANDWELL:
			if ((ctrl->value >= MIN_DWELL_TIME) &&
					(ctrl->value <= MAX_DWELL_TIME)) {
				radio->dwell_time_sec = ctrl->value;
			} else {
				pr_err("%s: scandwell period is not valid\n", __func__);
				retval = -EINVAL;
			}
			break;
		default:
			pr_info("%s id: %x in default\n",__func__, ctrl->id);
			return -EINVAL;
	}
end:
	pr_info("%s exit id: %x , ret: %d\n",__func__, ctrl->id, retval);
	return retval;
}


/*
 * si470x_vidioc_g_tuner - get tuner attributes
 */
static int si470x_vidioc_g_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	pr_info("%s enter\n",__func__);

	if (tuner->index != 0)
		return -EINVAL;
	if (!radio->status_rssi_auto_update) {
		retval = si470x_get_register(radio, STATUSRSSI);
		if (retval < 0)
			return retval;
	}

	/* driver constants */
	strcpy(tuner->name, "FM");
	tuner->type = V4L2_TUNER_RADIO;
	tuner->capability = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO |
		V4L2_TUNER_CAP_RDS | V4L2_TUNER_CAP_RDS_BLOCK_IO |
		V4L2_TUNER_CAP_HWSEEK_BOUNDED |
		V4L2_TUNER_CAP_HWSEEK_WRAP;

	if (radio->band == 0){
		tuner->rangelow  = 87.5 * FREQ_MUL;
		tuner->rangehigh = 108 * FREQ_MUL;
	} else if (radio->band == 1 ){
		tuner->rangelow  = 76 * FREQ_MUL;
		tuner->rangehigh = 108 * FREQ_MUL;
	} else if (radio->band == 2) {
		tuner->rangelow  = 76 * FREQ_MUL;
		tuner->rangehigh = 90 * FREQ_MUL;
	} else {
		tuner->rangelow  = 87.5 * FREQ_MUL;
		tuner->rangehigh = 108 * FREQ_MUL;
	}

	/* stereo indicator == stereo (instead of mono) */
	if ((radio->registers[STATUSRSSI] & STATUSRSSI_ST) == 0)
		tuner->rxsubchans = V4L2_TUNER_SUB_MONO;
	else
		tuner->rxsubchans = V4L2_TUNER_SUB_STEREO;
	/* If there is a reliable method of detecting an RDS channel,
	   then this code should check for that before setting this
	   RDS subchannel. */
	tuner->rxsubchans |= V4L2_TUNER_SUB_RDS;

	/* mono/stereo selector */
	if ((radio->registers[POWERCFG] & POWERCFG_MONO) == 0)
		tuner->audmode = V4L2_TUNER_MODE_STEREO;
	else
		tuner->audmode = V4L2_TUNER_MODE_MONO;

	/* min is worst, max is best; signal:0..0xffff; rssi: 0..0xff */
	/* measured in units of dbµV in 1 db increments (max at ~75 dbµV) */
	tuner->signal = (radio->registers[STATUSRSSI] & STATUSRSSI_RSSI);

	if (tuner->signal > 0x4B)
		tuner->signal = 0x4B;

	pr_info("%s tuner->signal:%x\n", __func__, tuner->signal);
	/* automatic frequency control: -1: freq to low, 1 freq to high */
	/* AFCRL does only indicate that freq. differs, not if too low/high */
	tuner->afc = (radio->registers[STATUSRSSI] & STATUSRSSI_AFCRL) ? 1 : 0;

	pr_info("%s exit\n",__func__);

	return retval;
}


/*
 * si470x_vidioc_s_tuner - set tuner attributes
 */
static int si470x_vidioc_s_tuner(struct file *file, void *priv,
		const struct v4l2_tuner *tuner)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	int band;

	pr_info("%s enter\n",__func__);

	if (tuner->index != 0)
		return -EINVAL;

	/* mono/stereo selector */
	switch (tuner->audmode) {
		case V4L2_TUNER_MODE_MONO:
			radio->registers[POWERCFG] |= POWERCFG_MONO;  /* force mono */
			break;
		case V4L2_TUNER_MODE_STEREO:
		default:
			radio->registers[POWERCFG] &= ~POWERCFG_MONO; /* try stereo */
			break;
	}

	retval = si470x_set_register(radio, POWERCFG);

	pr_info("%s low:%d high:%d\n", __func__, tuner->rangelow, tuner->rangehigh);

	/* set band */
	if (tuner->rangelow || tuner->rangehigh) {
		for (band = 0; band < ARRAY_SIZE(bands); band++) {
			if (bands[band].rangelow  == tuner->rangelow &&
					bands[band].rangehigh == tuner->rangehigh)
				break;
		}
		if (band == ARRAY_SIZE(bands)){
			pr_err("%s err\n",__func__);
			band = 0;
		}
	} else
		band = 0; /* If nothing is specified seek 87.5 - 108 Mhz */

	if (radio->band != band) {
		retval = si470x_set_band(radio, band);
		if (retval < 0)
			pr_err("%s fail to set band\n",__func__);
		else
			radio->band = band;
	}

	pr_info("%s exit\n", __func__);

	return retval;
}

/*
 * si470x_vidioc_g_frequency - get tuner or modulator radio frequency
 */
static int si470x_vidioc_g_frequency(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	pr_info("%s enter\n", __func__);

	freq->type = V4L2_TUNER_RADIO;
	retval = si470x_get_freq(radio, &freq->frequency);
	return retval;
}


/*
 * si470x_vidioc_s_frequency - set tuner or modulator radio frequency
 */
static int si470x_vidioc_s_frequency(struct file *file, void *priv,
		const struct v4l2_frequency *freq)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval;

	pr_info("%s enter freq:%d\n",__func__, freq->frequency);

	if (freq->frequency < bands[radio->band].rangelow ||
			freq->frequency > bands[radio->band].rangehigh) {
		/* Switch to band 1 which covers everything we support */
		retval = si470x_set_band(radio, 1);
		if (retval){
			pr_err("%s set band fial\n", __func__);
			return retval;
		}
	}
	return si470x_set_freq(radio, freq->frequency);
}


/*
 * si470x_vidioc_s_hw_freq_seek - set hardware frequency seek
 */
static int si470x_vidioc_s_hw_freq_seek(struct file *file, void *priv,
		const struct v4l2_hw_freq_seek *seek)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	pr_info("%s enter\n",__func__);

	if (file->f_flags & O_NONBLOCK)
		return -EWOULDBLOCK;

	radio->is_search_cancelled = false;

	if (radio->g_search_mode == SEEK) {
		/* seek */
		pr_info("%s starting seek\n",__func__);
		radio->seek_tune_status = SEEK_PENDING;
		retval = si470x_set_seek(radio, seek->seek_upward, seek->wrap_around);
		si470x_q_event(radio, SILABS_EVT_TUNE_SUCC);
		radio->seek_tune_status = NO_SEEK_TUNE_PENDING;
		si470x_q_event(radio, SILABS_EVT_SCAN_NEXT);
	} else if ((radio->g_search_mode == SCAN) ||
			(radio->g_search_mode == SCAN_FOR_STRONG)) {
		/* scan */
		if (radio->g_search_mode == SCAN_FOR_STRONG) {
			pr_info("%s starting search list\n",__func__);
			memset(&radio->srch_list, 0,
					sizeof(struct si470x_srch_list_compl));
		} else {
			pr_info("%s starting scan\n",__func__);
		}
		si470x_search(radio, 1);
	} else if ((radio->g_search_mode == SEEK_STOP) ){
		pr_info("%s seek stop\n", __func__);
		radio->registers[POWERCFG] &= ~POWERCFG_SEEK;
		retval = si470x_set_register(radio, POWERCFG);
	} else {
		retval = -EINVAL;
		pr_err("In %s, invalid search mode %d\n",
				__func__, radio->g_search_mode);
	}

	return retval;
}

/*
 * si470x_vidioc_enum_freq_bands - enumerate supported bands
 */
static int si470x_vidioc_enum_freq_bands(struct file *file, void *priv,
					 struct v4l2_frequency_band *band)
{
	pr_info("%s enter\n",__func__);

	if (band->tuner != 0)
		return -EINVAL;
	if (band->index >= ARRAY_SIZE(bands))
		return -EINVAL;
	*band = bands[band->index];
	return 0;
}

static int si470x_vidioc_g_fmt_type_private(struct file *file, void *priv,
						struct v4l2_format *f)
{
	return 0;

}

/*
 * si470x_ioctl_ops - video device ioctl operations
 */
static const struct v4l2_ioctl_ops si470x_ioctl_ops = {
	.vidioc_querycap			= si470x_vidioc_querycap,
	.vidioc_s_ctrl				= si470x_s_ctrl,
	.vidioc_g_ctrl 				= si470x_g_ctrl,
	.vidioc_g_tuner				= si470x_vidioc_g_tuner,
	.vidioc_s_tuner				= si470x_vidioc_s_tuner,
	.vidioc_g_frequency			= si470x_vidioc_g_frequency,
	.vidioc_s_frequency			= si470x_vidioc_s_frequency,
	.vidioc_s_hw_freq_seek		= si470x_vidioc_s_hw_freq_seek,
	.vidioc_dqbuf       		= si470x_vidioc_dqbuf,
	.vidioc_enum_freq_bands 	= si470x_vidioc_enum_freq_bands,
	.vidioc_g_fmt_type_private	= si470x_vidioc_g_fmt_type_private,
	.vidioc_subscribe_event 	= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static const struct v4l2_file_operations si470x_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32	= v4l2_compat_ioctl32,
#endif
	.read			= si470x_fops_read,
	.poll			= si470x_fops_poll,
	.open			= si470x_fops_open,
	.release		= si470x_fops_release,
};

/*
 * si470x_viddev_template - video device interface
 */
struct video_device si470x_viddev_template = {
	.fops			= &si470x_fops,
	.name			= DRIVER_NAME,
	.release		= video_device_release_empty,
	.ioctl_ops		= &si470x_ioctl_ops,
};

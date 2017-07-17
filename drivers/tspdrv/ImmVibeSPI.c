/*
** =============================================================================
**
** File: ImmVibeSPI.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
** $Revision$
**
** Copyright (c) 2007-2013 Immersion Corporation. All Rights Reserved.
**
** This file contains Original Code and/or Modifications of Original Code
** as defined in and that are subject to the GNU Public License v2 -
** (the 'License'). You may not use this file except in compliance with the
** License. You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
** the License for the specific language governing rights and limitations
** under the License.
**
** =============================================================================
*/

#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

#include "../staging/android/timed_output.h"
#include <linux/time.h>

#define TIMED_OUTPUT_DEVICE_NAME "vibrator"
#define TIMED_OUTPUT_TIMEOUT 10000
#define QPNP_HAP_VMAX_MIN_MV 116

#define DEVICE_NAME "PMI8996"

/*
** This SPI supports only one actuator.
*/
/* #error Please Set NUM_ACTUATORS to the number of actuators supported by this SPI. */
#define NUM_ACTUATORS       1

#define QPNP_HAP_VMAX_MIN_MV 116

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer);

extern int qpnp_haptic_timed_vmax(struct timed_output_dev *dev, int value, int duration);
//extern int qpnp_hap_play_byte(u8 data, bool on);
static struct timed_output_dev *tdev = 0;
static int vmax_rng = 0;
static VibeUInt8 nActuatorIndex = 0;
static int old_level = 0;
struct timespec curr_tm;

/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
    pr_err("%s: enter, nActuatorIndex : %d\n", __func__, nActuatorIndex);

    /* Disable amp */
    /* To be implemented with appropriate hardware access macros */

	/* handled automatically by qpnp-haptic driver */

    return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
    pr_err("%s: enter, nActuatorIndex : %d\n", __func__, nActuatorIndex);

    /* Reset PWM frequency */
    /* To be implemented with appropriate hardware access macros */

    /* Set duty cycle to 50% */
    /* To be implemented with appropriate hardware access macros */

    /* Enable amp */
    /* To be implemented with appropriate hardware access macros */

	/* handled automatically by qpnp-haptic driver */

    return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
mmVibeSPI_ForceOut_AmpDisableImmVibeSPI_ForceOut_AmpDisable*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
    /* Disable amp */
//    ImmVibeSPI_ForceOut_AmpDisable(0);

    /* Set PWM frequency */
    /* To be implemented with appropriate hardware access macros */

    /* Set duty cycle to 50% */
    /* To be implemented with appropriate hardware access macros */

    tdev = timed_output_dev_find_by_name(TIMED_OUTPUT_DEVICE_NAME);

    if (!tdev) {
        DbgOutErr(("%s: could not find timed_output_dev \"%s\"", __func__, TIMED_OUTPUT_DEVICE_NAME));
        return VIBE_E_FAIL;
    }

    /* retrieve initial vmax setting (range of actuator) and set initial strength to minimum */
    vmax_rng = qpnp_haptic_timed_vmax(tdev, 0, 0);

    return tdev ? VIBE_S_SUCCESS : VIBE_E_FAIL;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{

    /* Disable amp */
//    ImmVibeSPI_ForceOut_AmpDisable(0);
    VibeUInt8 buffer = 0;

    ImmVibeSPI_ForceOut_SetSamples(nActuatorIndex, 8, 1, &buffer);


    /* Set PWM frequency */
    /* To be implemented with appropriate hardware access macros */

    /* Set duty cycle to 50% */
    /* To be implemented with appropriate hardware access macros */

    return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set force output, and enable amp if required
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
    /* 
    ** For LRA:
    **      nBufferSizeInBytes should be equal to 1 if nOutputSignalBitDepth is equal to 8
    **      nBufferSizeInBytes should be equal to 2 if nOutputSignalBitDepth is equal to 16
    */

    /* Below based on assumed 8 bit PWM, other implementation are possible */

    /* M = 1, N = 256, 1 <= nDutyCycle <= (N -M) */

    /* Output force: nForce is mapped from [-127, 127] to [1, 255] */
    /* To be implemented with appropriate hardware access macros */
    int level = 0;
	int level_v = 0;

    if (!tdev) {
        pr_err("%s: tdev is NULL\n", __func__);

        return VIBE_E_FAIL;
    }

    if (!pForceOutputBuffer) {
        pr_err("%s: pForceOutputBuffer is NULL\n", __func__);

        return VIBE_E_FAIL;
    }
    if (nOutputSignalBitDepth == 8 && nBufferSizeInBytes == 1) {
        level = (signed char)(*pForceOutputBuffer);
    } else if (nOutputSignalBitDepth == 16 && nBufferSizeInBytes == 2) {
        level = ((signed short)(*((signed short*)(pForceOutputBuffer)))) >> 8;
    } else {
        pr_debug("%s: invalid set samples format: %d %d\n", __func__, nOutputSignalBitDepth, nBufferSizeInBytes);
        return VIBE_E_FAIL;
    }

	getnstimeofday(&curr_tm);

	if (level <= 0)
		level_v = 0;
	else {
		level_v = level * vmax_rng / 127;
		level_v = level_v / QPNP_HAP_VMAX_MIN_MV;
		level_v = level_v * QPNP_HAP_VMAX_MIN_MV + QPNP_HAP_VMAX_MIN_MV;
	}

//    pr_err("%s: tdev->name : %s, pForceOutputBuffer : %d\n", __func__, tdev->name, *pForceOutputBuffer);
//    pr_err("%s: level : %d, vmax_rng : %d, value : %d\n", __func__, level, vmax_rng, level_v);
//    pr_err("%s: current time : %lu\n", __func__, curr_tm.tv_nsec / 1000000);

	if (old_level == level_v) {
//		pr_err("%s: level is not updated. old_level : %d, level : %d\n", __func__, old_level, level);
		return VIBE_S_SUCCESS;
	}
    qpnp_haptic_timed_vmax(tdev, level_v, TIMED_OUTPUT_TIMEOUT);

	old_level = level_v;
//	qpnp_hap_play_byte(level, level);

    return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
    pr_err("%s: enter, nActuatorIndex : %d, nFrequencyParameterID : %d, nFrequencyParameterValue : %d\n", __func__,nActuatorIndex,nFrequencyParameterID,nFrequencyParameterValue);

#if 0 
    /* 
    ** The following code is provided as sample. If enabled, it will allow device 
    ** frequency parameters tuning via the ImmVibeSetDeviceKernelParameter API.
    ** Please modify as required. 
    */
    switch (nFrequencyParameterID)
    {
        case VIBE_KP_CFG_FREQUENCY_PARAM1:
            /* Update frequency parameter 1 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM2:
            /* Update frequency parameter 2 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM3:
            /* Update frequency parameter 3 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM4:
            /* Update frequency parameter 4 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM5:
            /* Update frequency parameter 5 */
            break;

        case VIBE_KP_CFG_FREQUENCY_PARAM6:
            /* Update frequency parameter 6 */
            break;
    }
#endif

    return VIBE_S_SUCCESS;
}

/*
** Called to save an IVT data file (pIVT) to a file (szPathName)
*/

//IMMVIBESPIAPI VibeStatus ImmVibeSPI_IVTFile_Save(const VibeUInt8 *pIVT, VibeUInt32 nIVTSize, const char *szPathname)
//{

    /* To be implemented */

//    return VIBE_S_SUCCESS;
//}

/*
** Called to delete an IVT file
*/
//IMMVIBESPIAPI VibeStatus ImmVibeSPI_IVTFile_Delete(const char *szPathname)
//{

    /* To be implemented */

//    return VIBE_S_SUCCESS;
//}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{

    if ((!szDevName) || (nSize < 1)) return VIBE_E_FAIL;

    strncpy(szDevName, DEVICE_NAME, nSize-1);
    szDevName[nSize - 1] = '\0';    /* make sure the string is NULL terminated */

    return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to get the number of actuators
*/
//IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetNum(void)
//{
//    return NUM_ACTUATORS;
//}

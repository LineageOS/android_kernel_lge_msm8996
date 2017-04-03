/*
 *  felica_pon.c
 *
 */

/*
 *    INCLUDE FILES FOR MODULE
 */
#include "felica_pon.h"
#include "felica_gpio.h"

/*
 *  DEFINE
 */

/*
 *    INTERNAL DEFINITION
 */

static int isopen = 0; // 0 : No open 1 : Open

/*
 *    FUNCTION DEFINITION
 */

/*
* Description : MFC calls this function using close method(void open()) of FileOutputStream class
*               When this fuction is excuted, set PON to Low.
* Input : None
* Output : Success : 0 Fail : Other
*/
static int felica_pon_open (struct inode *inode, struct file *fp)
{
  int rc = 0;

  if(1 == isopen)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_open - already open \n");
    return -1;
  }
  else
  {
    FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_open \n");
    isopen = 1;
  }

  return rc;
}

/*
* Description : MFC calls this function using write method(void write(int oneByte)) of FileOutputStream class
* Input : PON low : 0 PON high : 1
* Output : Success : 0 Fail : Other
*/
static ssize_t felica_pon_write(struct file *fp, const char *buf, size_t count, loff_t *pos)
{
  int rc = 0;
  int SetValue = 0;

  if(NULL == fp)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR file \n");
    return -1;
  }

  if(NULL == buf)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR buf \n");
    return -1;
  }

  if(1 != count)
  {
    FELICA_DEBUG_MSG("[FELICA_PON]ERROR count \n");
    return -1;
  }

  if(NULL == pos)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR file \n");
    return -1;
  }

  rc = copy_from_user(&SetValue, (void*)buf, count);
  if(rc)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR - copy_from_user \n");
    return rc;
  }

  if((GPIO_LOW_VALUE != SetValue)&&(GPIO_HIGH_VALUE != SetValue))
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR - SetValue is out of range \n");
    return -1;
  }
  else if(GPIO_LOW_VALUE != SetValue)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ========> ON \n");
  }
  else if(GPIO_HIGH_VALUE != SetValue)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] <======== OFF \n");
  }

  felica_gpio_open(GPIO_FELICA_PON, GPIO_DIRECTION_OUT, SetValue);

  mdelay(100);

  felica_gpio_write(GPIO_FELICA_PON, SetValue);

  mdelay(100);

  return 1;
}
static ssize_t felica_pon_read(struct file *fp, char *buf, size_t count, loff_t *pos)
{
  int rc = 0;
  int getvalue = GPIO_LOW_VALUE;

  if(NULL == fp)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR fp \n");
    return -1;
  }

  if(NULL == buf)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR buf \n");
    return -2;
  }

  if(1 != count)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR count \n");
    return -3;
  }

  if(NULL == pos)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR file \n");
    return -4;
  }

  getvalue = felica_gpio_read(GPIO_FELICA_PON);

  if((GPIO_LOW_VALUE != getvalue)&&(GPIO_HIGH_VALUE != getvalue))
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR - getvalue is out of range \n");
    return -5;
  }

  FELICA_DEBUG_MSG("[FELICA_PON] PON status : %d \n", getvalue);

  rc = copy_to_user((void*)buf, (void*)&getvalue, count);
  if(rc)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] ERROR - copy_to_user \n");
    return rc;
  }

  return count;
}

/*
* Description : MFC calls this function using close method(void close()) of FileOutputStream class
*               When this fuction is excuted, set PON to Low.
* Input : None
* Output : Success : 0 Fail : Other
*/
static int felica_pon_release (struct inode *inode, struct file *fp)
{
  if(0 == isopen)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_release - not open \n");
    return -1;
  }
  else
  {
    FELICA_DEBUG_MSG("[FELICA_PON] felica_pon_release \n");
    isopen = 0;
  }

  return 0;
}

/*
 *    STRUCT DEFINITION
 */

static struct file_operations felica_pon_fops =
{
  .owner    = THIS_MODULE,
  .open      = felica_pon_open,
  .read 		 = felica_pon_read,
  .write    = felica_pon_write,
  .release  = felica_pon_release,
};

static struct miscdevice felica_pon_device =
{
  .minor = MINOR_NUM_FELICA_PON,
  .name = FELICA_PON_NAME,
  .fops = &felica_pon_fops,
};

static int felica_pon_init(void)
{
  int rc = 0;

  rc = misc_register(&felica_pon_device);
  if (rc)
  {
    FELICA_DEBUG_MSG("[FELICA_PON] FAIL!! can not register felica_pon \n");
    return rc;
  }

  return 0;
}

static void felica_pon_exit(void)
{
  misc_deregister(&felica_pon_device);
}

module_init(felica_pon_init);
module_exit(felica_pon_exit);

MODULE_LICENSE("Dual BSD/GPL");

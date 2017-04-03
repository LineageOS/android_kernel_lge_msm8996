/*
 *  felicagpio.c
 *
 */

/*
 *    INCLUDE FILES FOR MODULE
 */
#include <linux/gpio.h>

#include "felica_gpio.h"



/* Debug message feature */
/* #define FELICA_DEBUG_MSG */

/*
* Description :
* Input :
* Output :
*/
int felica_gpio_open(int gpionum, int direction, int value)
{
  int rc = 0;

  if(direction == GPIO_DIRECTION_IN)
  {
    rc = gpio_direction_input((unsigned)gpionum);

    if(rc)
    {
      FELICA_DEBUG_MSG("[FELICA] ERROR -  gpio_direction_input \n");
      return rc;
    }
  }
  else
  {
    rc = gpio_direction_output((unsigned)gpionum, value);

    if(rc)
    {
      FELICA_DEBUG_MSG("[FELICA] ERROR -  gpio_direction_output \n");
      return rc;
    }
  }

  return rc;
}

/*
* Description :
* Input :
* Output :
*/
void felica_gpio_write(int gpionum, int value)
{
  gpio_set_value(gpionum, value);
}

/*
* Description :
* Input :
* Output :
*/
int felica_gpio_read(int gpionum)
{
  return gpio_get_value(gpionum);
}

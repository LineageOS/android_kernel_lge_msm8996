#ifndef _PN547_LGE_HWADAPTER_H_
#define _PN547_LGE_HWADAPTER_H_

#include <linux/nfc/pn547_lge.h>

#include <linux/of_gpio.h>

#ifdef CONFIG_LGE_NFC_HW_ODIN
#include <linux/platform_data/gpio-odin.h>
#endif

#ifdef CONFIG_LGE_NFC_USE_PMIC
#include <linux/clk.h>
#if 0 // for msm8974
#include "../../arch/arm/mach-msm/clock-rpm.h"

#define D1_ID        2
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_d1, cxo_d1_a, D1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_d1_pin, cxo_d1_a_pin, D1_ID);
#endif

#endif

#ifdef CONFIG_LGE_NFC_ODIN_SMS_I2C

#define PN547_I2C_SLOT   4
#define PN547_SLAVE_ADDRESS 0x50  //0x28 << 1

static struct regmap_config pn547_regmap_config =
{
    .reg_bits = 8,
#ifndef MAX77819_PWM_EN
    .val_bits = 8,
#else
    .val_bits = 16,
#endif
    .val_format_endian = REGMAP_ENDIAN_NATIVE,
};

#endif

int pn547_get_hw_revision(void);
unsigned int pn547_get_irq_pin(struct pn547_dev *dev);
int pn547_gpio_to_irq(struct pn547_dev *dev);
void pn547_gpio_enable(struct pn547_dev *pn547_dev);
void pn547_shutdown_cb(struct pn547_dev *pn547_dev);

#ifdef CONFIG_LGE_NFC_USE_PMIC
void pn547_get_clk_source(struct i2c_client *pn547_client, struct pn547_dev *pn547_dev);
#endif

void pn547_parse_dt(struct device *dev, struct pn547_dev *pn547_dev);

#endif /* _PN547_LGE_HWADAPTER_H_ */

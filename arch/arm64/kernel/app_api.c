/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/cpu.h>
#include <linux/export.h>

static spinlock_t spinlock;
static DEFINE_PER_CPU(int, app_wa_applied);
static unsigned int app_wa_set[NR_CPUS];
static unsigned int app_wa_clear[NR_CPUS];

#define APP_SETTING_BIT 30

void set_app_setting_bit(uint32_t bit)
{
	uint64_t reg;
	unsigned long flags;
	int cpu;

	spin_lock_irqsave(&spinlock, flags);
	asm volatile("mrs %0, S3_1_C15_C15_0" : "=r" (reg));
	reg = reg | BIT(bit);
	isb();
	asm volatile("msr S3_1_C15_C15_0, %0" : : "r" (reg));
	isb();
	spin_unlock_irqrestore(&spinlock, flags);

	if (bit == APP_SETTING_BIT) {
		cpu = raw_smp_processor_id();
		app_wa_set[cpu]++;

		this_cpu_write(app_wa_applied, 1);
	}

}
EXPORT_SYMBOL(set_app_setting_bit);

void clear_app_setting_bit(uint32_t bit)
{
	uint64_t reg;
	unsigned long flags;
	int cpu;

	spin_lock_irqsave(&spinlock, flags);
	asm volatile("mrs %0, S3_1_C15_C15_0" : "=r" (reg));
	reg = reg & ~BIT(bit);
	isb();
	asm volatile("msr S3_1_C15_C15_0, %0" : : "r" (reg));
	isb();
	spin_unlock_irqrestore(&spinlock, flags);

	if (bit == APP_SETTING_BIT) {
		cpu = raw_smp_processor_id();
		app_wa_clear[cpu]++;

		this_cpu_write(app_wa_applied, 0);
	}

}
EXPORT_SYMBOL(clear_app_setting_bit);

static int __init init_app_api(void)
{
	spin_lock_init(&spinlock);
	return 0;
}
early_initcall(init_app_api);

#ifndef _HEADER_TRITON_H_
#define _HEADER_TRITON_H_
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/tick.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include <asm/cputime.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/kthread.h>

#include <linux/lib_triton.h>

#define MAX_CPUS_DEFLT    (8)

#define SYSFS_CUR_PL (0)
#define SYSFS_AFREQ  (1)
#define SYSFS_BFREQ  (2)
#define SYSFS_EN		(3)
#define SYSFS_ENF		(4)
#define SYSFS_DBG	(5)
struct io_dev {
	struct semaphore sem;
	struct cdev char_dev;
};

struct sys_info{
	int cur_policy;
	int enable;
#ifdef TR_DEBUG
	int enforce;
#endif
	int aevents;
	int bevents;
	int sm;
	int sw;
	int lsp;
	int debug;
};
struct cpuinfo {
	unsigned long prev_load;
	int prev_load_cnt;
	struct rw_semaphore sem;
	struct cpufreq_policy *policy;
};
struct config {
	int major;
	spinlock_t hotplug_lock;
	spinlock_t frequency_change_lock;
	bool	init_ok;
	int	dst_cpu;
	struct workqueue_struct         *fwq;
	struct delayed_work 	frequency_changed_wq;
	struct workqueue_struct         *ping_fwq;
	struct delayed_work		ping_wq;
	struct work_struct	 	sysfs_wq;
	int		sysfs_id;
	struct class 	            *class;
	struct kobject		    *kobject;
	struct io_dev        *tio_dev;
	struct sys_info   		sys;
	struct freq_info     freq_tx[NUM_CLUSTER];
	struct freq_info     freq_rx;
	struct perf_level	param[BIT_MAX];
	struct tunables		tunables;
#ifdef BMC
	struct tunables_opts tunables_opt[NUM_CLUSTER];
#endif
	struct comm comm;
};
void stack(int cpu, int freq);
void update_cpu_load(int cpu);
unsigned int cpufreq_restore_freq(unsigned long data);
extern int get_tstate(int cpu);
#ifdef CONFIG_LGE_PM_CANCUN
extern int get_cancun_status(void);
#endif
#endif

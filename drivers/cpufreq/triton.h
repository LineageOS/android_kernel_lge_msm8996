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

#define CPU_FREQ_TRANSITION 0
#define CPU_LOAD_TRANSITION 1
#define CPU_OWNER_TRANSITION 2
enum sysfs_notify_id {
	SYSFS_NOTIFY_CUR_POLICY,
	SYSFS_NOTIFY_AFREQ,
	SYSFS_NOTIFY_BFREQ,
	SYSFS_NOTIFY_ENABLE,
	SYSFS_NOTIFY_ENFORCE,
	SYSFS_NOTIFY_DEBUG
};
#ifdef FPS_BOOST
struct commit_sync
{
	u64 last_commit_ms;
	int drop_count;
	int sync_start;
	int drop_thres;
	int sync_duration;
};
#endif
struct io_dev {
	struct semaphore sem;
	struct cdev char_dev;
};

struct sysfs_notify_info{
	enum sysfs_notify_id notify_id;
	int cur_policy;
	int enable;
	int enforce;
	int aevents;
	int bevents;
	int debug;
};
struct governor_policy_info {
#ifndef SCHED_BUSY_SUPPORT
	u64 prev_wall_time;
	u64 prev_idle_time;
#endif
	struct rw_semaphore sem;
	struct cpufreq_policy *policy;
};
struct ioctl_data {
	struct sys_cmd_freq_req freq_per_cluster[NUM_CLUSTER];
	struct sys_cmd_perf_level_req perf_param[BIT_MAX];
	struct sys_cmd_comm_req common_req;
	struct sys_cmd_tunables_req tunables_param;
#ifdef BMC
	struct sys_cmd_tunables_bmc_req tunables_bmc_param[NUM_CLUSTER];
#endif
};
struct triton_platform_data {
	int major;
	enum sys_state_machine state;
	enum adjust_level level;
	spinlock_t hotplug_lock;
	spinlock_t frequency_change_lock;
	struct workqueue_struct *fwq;
	struct delayed_work frequency_changed_wq;
	struct workqueue_struct *ping_fwq;
	struct delayed_work ping_wq;
	struct work_struct sysfs_wq;
	struct class *class;
	struct kobject *kobject;
	struct io_dev *tio_dev;
	struct ioctl_data ioctl;
	struct sysfs_notify_info notify_info;
};
void stack(int cpu, int freq);
int triton_notify(unsigned int evt, unsigned int cpu, void *v);
unsigned int cpufreq_restore_freq(unsigned long data);
int check_current_cpu_owner(int cpu);
#ifdef CONFIG_LGE_PM_CANCUN
int get_cancun_status(void);
#endif
int cpufreq_interactive_governor_stat(int cpu);
struct cpufreq_policy *cpufreq_interactive_get_policy(int cpu);
#endif

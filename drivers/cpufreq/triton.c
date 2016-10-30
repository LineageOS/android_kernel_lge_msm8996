#include "triton.h"
#if (NUM_CLUSTER > 1)
static int lcluster_cores;
static int bcluster_cores;
static int lcluster_start;
static int bcluster_start;
#endif
static u64 ping_time, last_ping_time;
#define DEFAULT_PING_INTV (5000)
#define DEFAULT_PING_EXP (10000)

static int ping_expired = 0;

static struct triton_platform_data platform_data;
static DEFINE_PER_CPU(struct governor_policy_info, policy_info);

/* function prototypes */
static void create_sysfs(void);

#if (NUM_CLUSTER > 1)
#define CPU_NUM_BY_CLUSTER(c, f, l) \
	*f = (c << (CORES_PER_CLUSTER >>1)); \
	*l = (CORES_PER_CLUSTER << c);

#define CLUSTER_BY_CPU(c) ((c >=0) ? ((c < lcluster_cores) ? 0 : 1) : -1)
#define CLUSTER_BY_POLICY(p) ((p > NT) ? ((p < SSTB) ? 0 : 1) : -1)
#else
#define CPU_NUM_BY_CLUSTER(c, f, l) \
	*f = 0; \
    *l = MAX_CORES;
#define CLUSTER_BY_CPU(c) (0)
#define CLUSTER_BY_POLICY(p) ((p > NT) ? 1 : 0)
#endif


/*---------------------------------+
 *      |      little  |    big    |
 * SSTB |     allow    |  restrict |
 * --------------------------------+
 *  NOM |    restrict  |  allow    |
 *  STB |              |           |
 *---------------------------------+
 * 1@return value : allow
 * 0@return value : not allow
 */
int check_current_cpu_owner(int cpu)
{
        int cluster = CLUSTER_BY_CPU(cpu);
	int policy = platform_data.notify_info.cur_policy;

	if(!policy || cluster < 0)
		return 1;

	switch(policy) {
#if (NUM_CLUSTER > 1)
	case SSTB:
		return !cluster;
#endif
	case STB:
	case NOM:
		return cluster;
	default:
		return 1;
    }
    return 1;
}
static u64 get_busy(int cpu)
{
#ifdef SCHED_BUSY_SUPPORT
	return (u64)sched_get_busy(cpu);
#else
	u64 cur_wall_time, cur_idle_time;
	unsigned int delta_wall_time, delta_idle_time;
	unsigned int busy;
	struct governor_policy_info *pcpu = &per_cpu(policy_info, cpu);
	if(!pcpu)
		return 0;
	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, 0);
	delta_wall_time = (unsigned int)(cur_wall_time - pcpu->prev_wall_time);
	delta_idle_time = (unsigned int)(cur_idle_time - pcpu->prev_idle_time);

	if(!delta_wall_time || delta_wall_time  < delta_idle_time)
		return 0;
	busy = (((delta_wall_time - delta_idle_time) * 100) / delta_wall_time) * 200;
	pcpu->prev_wall_time = cur_wall_time;
	pcpu->prev_idle_time = cur_idle_time;
	return busy;
#endif
}

void update_cpufreq(int cpu, int freq)
{
	unsigned long flag = 0;
	int cluster = -1, fcpu, lcpu;
#ifdef CURR_HIST
	int i;
#endif
	struct sys_cmd_freq_req *pfreq;

	spin_lock_irqsave(&platform_data.frequency_change_lock,
					flag);
	cluster = CLUSTER_BY_CPU(cpu);
	CPU_NUM_BY_CLUSTER(cluster, &fcpu, &lcpu);
	if(cluster < 0) {
		pr_err("cluster is not valid..... -1\n");
		spin_unlock_irqrestore(&platform_data.frequency_change_lock, flag);
		return;
	}
	pfreq = &platform_data.ioctl.freq_per_cluster[cluster];
	pfreq->req_cluster = cluster;
	pfreq->req_cpu = cpu;
	pfreq->req_freq = freq;
#ifdef CURR_HIST
	for(i = 0 ; i < MAX_CORES; i++) {
		pfreq->each_load[i] = get_busy(i);
	}
#endif

	spin_unlock_irqrestore(&platform_data.frequency_change_lock, flag);

	switch(cluster) {
	case 0:
		sysfs_notify(platform_data.kobject, NULL, "aevents");
		break;
	case 1:
		sysfs_notify(platform_data.kobject, NULL, "bevents");
		break;
	default:
		break;
	}
}

int triton_notify(unsigned int evt, unsigned int cpu, void *v)
{
	int ret = 0;
	switch(evt) {
	case CPU_FREQ_TRANSITION:
		update_cpufreq(cpu, *(int *)v);
		break;
	case CPU_LOAD_TRANSITION:
		break;
	case CPU_OWNER_TRANSITION:
/* important! */
		ret =  check_current_cpu_owner(cpu);
		break;
	}
	return ret;
}
static int get_dst_cpu(int pol, int *cl)
{
	int cpu, fcpu, lcpu;
	struct device *dev;

	*cl = CLUSTER_BY_POLICY(pol);

	if(*cl < 0)
		return -1;

	CPU_NUM_BY_CLUSTER(*cl, &fcpu, &lcpu);

	for(cpu = fcpu; cpu < lcpu ; cpu++) {
		dev = get_cpu_device(cpu);
		if(dev && !cpu_is_offline(dev->id)) {
			return cpu;
		}
	}
	return -1;
}

static void sysfs_set_noti_data(enum sysfs_notify_id notify)
{
	platform_data.notify_info.notify_id = notify;
	schedule_work(&platform_data.sysfs_wq);
}

static int start_kpolicy(int kpolicy)
{
	unsigned long flag;

	spin_lock_irqsave(&platform_data.hotplug_lock, flag);

	platform_data.notify_info.cur_policy = kpolicy;

	if(platform_data.state == FREEZE && kpolicy > NT) {
		platform_data.state = RUNNING;
		platform_data.level = CEILING;
		queue_delayed_work(platform_data.fwq,
					&platform_data.frequency_changed_wq,
					usecs_to_jiffies(platform_data.ioctl.tunables_param.tunable_timer_rate_us));

		sysfs_set_noti_data(SYSFS_NOTIFY_CUR_POLICY);
	}
	spin_unlock_irqrestore(&platform_data.hotplug_lock, flag);
	return 0;
}

#ifdef FPS_BOOST
extern u64 last_commit_ms;
static struct commit_sync sync;
#endif
static int stop_kpolicy(void)
{
	int last_policy, cl = -1, dest_cpu = -1;
	struct device *dev;
	if(!platform_data.notify_info.cur_policy ||
			platform_data.state == FREEZE)
		return 0;
	last_policy = platform_data.notify_info.cur_policy;
	platform_data.notify_info.cur_policy = NT;
	platform_data.state = FREEZE;
	platform_data.level = CEILING;
#ifdef FPS_BOOST
	platform_data.state &= ~BOOST;
	sync.drop_count= 0;
#endif
	cancel_delayed_work(&platform_data.frequency_changed_wq);

	/* confirmed stop */
	sysfs_set_noti_data(SYSFS_NOTIFY_CUR_POLICY);

	/* restore frequency of last policy */
	dest_cpu = get_dst_cpu(last_policy, &cl);
	if(dest_cpu < 0)
		return -EFAULT;

	dev = get_cpu_device(dest_cpu);
	if(dev && !cpu_is_offline(dev->id)) {
		cpufreq_restore_freq(dest_cpu);
	}
	return 0;
}
static int set_dst_cpu(int freq, int cpu)
{
	struct device *dev = get_cpu_device(cpu);
	struct cpufreq_policy *policy = cpufreq_interactive_get_policy(cpu);
	struct governor_policy_info *pcpu = &per_cpu(policy_info, cpu);

	if(!dev || cpu_is_offline(dev->id)) {
		pr_err("cpu%d is not valid\n", cpu);
		goto noti;
	}

	if(!policy) {
		pr_err("cpu%d policy is NULL\n", cpu);
		goto noti;
	}
	if(!policy->governor_data) {
		pr_err("gov(%d) data is NULL\n", cpu);
		goto noti;
	}
	if(policy != pcpu->policy) {
		pcpu->policy = policy;
		pr_err("comparing policy : it's  NOT matched\n");
		goto noti;
	}
	if(!cpufreq_interactive_governor_stat(cpu)) {
		if(policy->cur != freq)
			cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_H);
		return 0;
	}
noti:
	return -EINVAL;
}

#ifdef BMC
#define ABS_DIFF(x,y) ((x > y)? (x-y) : (y-x))
#ifdef FPS_BOOST
static int fps_boost
(struct sys_cmd_tunables_bmc_req *bmc_ptr)
{
	int ret = 0;

	if(!sync.drop_thres)
		return ret;

	if(platform_data.state & BOOST) {

		u64 curr = ktime_to_ms(ktime_get());

		if(curr - sync.sync_start <= sync.sync_duration) {
			if(sync.last_commit_ms == last_commit_ms) {
				sync.sync_start = ktime_to_ms(ktime_get());
				sync.sync_duration += bmc_ptr->duration;
			}
			return 1;
		} else {
			platform_data.state &= ~BOOST;
			sync.sync_start = 0;
			sync.drop_count = 0;
			sync.sync_duration = bmc_ptr->duration;
		}
	}
	if(last_commit_ms == sync.last_commit_ms) {
		++sync.drop_count;
	} else {
		if(sync.drop_count > 0)
			--sync.drop_count;
	}
	if(sync.drop_count >= sync.drop_thres) {
		platform_data.state = RUNNING | BOOST;
		sync.sync_start= ktime_to_ms(ktime_get());
		return 1;
	}
	sync.last_commit_ms = last_commit_ms;

	return ret;
}
#endif
static int get_opt_frequency(int cl, int lvl)
{
	int cpu, lcpu, fcpu, freq = 0;
	unsigned int load[CORES_PER_CLUSTER];
	unsigned int max = 0, min = UINT_MAX;
	unsigned int diff, /*coeff, */ratio, dyn_range;
	struct sys_cmd_tunables_bmc_req *bmc_param;
	u32 calcfreq;

	bmc_param = &platform_data.ioctl.tunables_bmc_param[cl];
	freq = platform_data.ioctl.freq_per_cluster[cl].req_freq;
#ifdef FPS_BOOST
	if(bmc_param->thres > 0 && fps_boost(bmc_param)) {
		return ((freq <= bmc_param->turbo) ?
				bmc_param->turbo : freq);
	}
#endif

	if(freq < bmc_param->minturbo)
		return bmc_param->minturbo;

	CPU_NUM_BY_CLUSTER(cl, &fcpu, &lcpu);

	for(cpu = fcpu; cpu < lcpu; cpu++) {
		if(cpu_is_offline(cpu)){
			pr_debug("cpu%d offlined \n", cpu);
			return -1;
		}
		ratio = ((u64)get_busy(cpu) * 100) /20000;

		load[cpu-fcpu] = (unsigned int)ratio;

		if(load[cpu-fcpu] < min)
			min = load[cpu-fcpu];
		if(load[cpu-fcpu] > max)
			max = load[cpu-fcpu];
	}

	diff = ABS_DIFF(max, min);

    dyn_range = bmc_param->maxturbo - bmc_param->minturbo;

	calcfreq = (dyn_range * diff) /100;

	freq = (int)(freq - calcfreq);

	if(freq <  bmc_param->minturbo)
		freq = bmc_param->minturbo;
	if(freq > bmc_param->maxturbo)
		freq = bmc_param->maxturbo;
	return freq;
}
#endif

static int get_dst_freq(int dest_cpu, int lvl)
{
	int cl = -1;

	struct device *dev;

	cl = CLUSTER_BY_CPU(dest_cpu);
	if(cl < 0)
		return -1;

	dev = get_cpu_device(dest_cpu);
	if(!dev || cpu_is_offline(dev->id)) {
		pr_err("%s cpu%d is not valid\n", __func__, dest_cpu);
		return -1;
	}
	if(cpufreq_interactive_governor_stat(dest_cpu)) {
		pr_err("%s gov %d is not valid\n", __func__, dest_cpu);
		return -1;
	}
#ifdef BMC
	return get_opt_frequency(cl, lvl);
#else
	return req_freq;
#endif
}
static void sysfs_noti_process(struct work_struct *sysfs_noti_work)
{
	struct triton_platform_data *platform_data = container_of(sysfs_noti_work,
							struct triton_platform_data,
							sysfs_wq);
	switch(platform_data->notify_info.notify_id) {
	case SYSFS_NOTIFY_CUR_POLICY:
		sysfs_notify(platform_data->kobject, NULL, "cur_policy");
		break;
	case SYSFS_NOTIFY_AFREQ:
		sysfs_notify(platform_data->kobject, NULL, "aevents");
		break;
	case SYSFS_NOTIFY_BFREQ:
		sysfs_notify(platform_data->kobject, NULL, "bevents");
		break;
	case SYSFS_NOTIFY_ENABLE:
		sysfs_notify(platform_data->kobject, NULL, "enable");
		break;
	case SYSFS_NOTIFY_ENFORCE:
		sysfs_notify(platform_data->kobject, NULL, "enforce");
		break;
	case SYSFS_NOTIFY_DEBUG:
		sysfs_notify(platform_data->kobject, NULL, "debug");
		break;
	}
}
static void ping_process(struct work_struct *work)
{
	int ping_exp = platform_data.ioctl.tunables_param.tunable_ping_exp;
	int ping_intv = platform_data.ioctl.tunables_param.tunable_ping_intv;

	if(ping_exp == 0)
		ping_exp = DEFAULT_PING_EXP;
	if(ping_intv == 0)
		ping_intv = DEFAULT_PING_INTV;

	last_ping_time = ktime_to_ms(ktime_get());
	if(last_ping_time - ping_time >= ping_exp) {
		ping_time = last_ping_time;
		ping_expired = 1;
		stop_kpolicy();
	}
	queue_delayed_work(platform_data.ping_fwq,
					&platform_data.ping_wq,
					msecs_to_jiffies(ping_intv));
}
static void frequency_process(struct work_struct *work)
{
	struct triton_platform_data *platform_data = container_of(work,
				struct triton_platform_data,
				frequency_changed_wq.work);
	int dst_cpu = -1, cl = -1;
	int opt_freq = 0;
	int ccm = 0;
#ifdef CONFIG_LGE_PM_CANCUN
	ccm = get_cancun_status();
#endif
	if(!platform_data)
		goto exit;
	if(!platform_data->notify_info.enable)
		goto exit;
	if(!platform_data->state & RUNNING)
		goto exit;
	if(ccm) {
		pr_info("exclusive working \n");
		goto exit;
	}
	dst_cpu = get_dst_cpu(platform_data->notify_info.cur_policy, &cl);
	if(dst_cpu < 0) {
		pr_err("dst cpu(%d) is not valid\n", dst_cpu);
		goto exit;
	}
	opt_freq = get_dst_freq(dst_cpu,
						platform_data->level);
	if(!set_dst_cpu(opt_freq, dst_cpu)){
		platform_data->level ^= 1;
		goto rearm;
	}
	goto exit;
rearm:
	queue_delayed_work(platform_data->fwq,
			&platform_data->frequency_changed_wq,
			usecs_to_jiffies(platform_data->ioctl.tunables_param.tunable_timer_rate_us));
	return;
exit:
	stop_kpolicy();
	return;
}
static long validate(unsigned int *cmd, unsigned long *arg)
{
	long ret = 0, err_val = 0;
	if((_IOC_TYPE(*cmd) != MAGIC_NUM)) {
		return -EFAULT;
	}
	if(_IOC_NR(*cmd) >= REQ_MAX) {
		return -EFAULT;
	}
	if (_IOC_DIR(*cmd) & _IOC_READ) {
		err_val = !access_ok(VERIFY_WRITE, (void __user *)*arg,
				_IOC_SIZE(*cmd));
	} else if (_IOC_DIR(*cmd) & _IOC_WRITE) {
		err_val = !access_ok(VERIFY_READ, (void __user *)*arg,
				_IOC_SIZE(*cmd));
	}
	if (err_val) {
		return -EFAULT;
	}
	return ret;
}


static int update_commons(unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;
	int request, value;
	ret = copy_from_user(&platform_data.ioctl.common_req, argp,
			sizeof(struct sys_cmd_comm_req));
	if(ret)
		return -ENOTTY;
	request = platform_data.ioctl.common_req.req;
	value = platform_data.ioctl.common_req.val;

	switch(request) {
		/* policy */
	case COMM_POLICY:
		if(!value) {
			stop_kpolicy();
			break;
		}
		start_kpolicy(value);
		break;
	/* enable */
	case COMM_ENABLE:
		platform_data.notify_info.enable = value;
		break;
	/* enforce */
	case COMM_ENFORCE:
		platform_data.notify_info.enforce = value;
		break;
	default:
		break;
	}
	return ret;
}

static int update_sys_frequency
		(unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;
	struct sys_cmd_freq_req freq;

	ret = copy_from_user(&freq,
				argp,
				sizeof(struct sys_cmd_freq_req));
	if(ret)
		return -ENOTTY;

	ret = copy_to_user((void __user*)arg,
			&platform_data.ioctl.freq_per_cluster[freq.req_cluster],
		    sizeof(struct sys_cmd_freq_req));

	return ret;
}
#ifdef CURR_HIST
static int update_sys_cpu_load(unsigned long arg)
{
	struct sys_cmd_freq_req pfreq;
	int i, ret = 0;
	for(i = 0 ; i < MAX_CORES; i++) {
		pfreq.each_load[i] = get_busy(i);
		pfreq.cstate[i] = sched_get_cpu_cstate(i);
	}
	ret = copy_to_user((void __user*)arg,
			&pfreq,
		    sizeof(struct sys_cmd_freq_req));
	return ret;
}
#endif

static int update_sys_tunables(unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;
	ret = copy_from_user(&platform_data.ioctl.tunables_param,
			             argp,
			             sizeof(struct sys_cmd_tunables_req));
	if(ret)
		return -ENOTTY;
	return ret;
}
#ifdef BMC
static int update_sys_opt_tunables(unsigned long arg)
{
	void __user *argp = (void __user *)arg;
#ifdef FPS_BOOST
	struct sys_cmd_tunables_bmc_req *bmc_req;
#endif
	int ret = 0;

	ret = copy_from_user(&platform_data.ioctl.tunables_bmc_param,
			         argp,
			         sizeof(struct sys_cmd_tunables_bmc_req) * NUM_CLUSTER);
	if(ret)
		return -ENOTTY;

#ifdef FPS_BOOST
	bmc_req = &platform_data.ioctl.tunables_bmc_param[BIT_BIG];
	sync.sync_duration = bmc_req->duration;
	sync.drop_thres = bmc_req->thres;
#endif
	return ret;

}
#endif
static int update_sys_perf_level(unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;
	ret = copy_from_user(&platform_data.ioctl.perf_param,
			argp,
			sizeof(struct sys_cmd_perf_level_req) * BIT_MAX);
	if(ret)
		return -ENOTTY;

	return ret;
}

static long io_progress(struct file *filep, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;
	ret = validate(&cmd, &arg);

	if(ret) {
		return -EFAULT;
	}

	switch(cmd) {
	case IOCTL_AFREQ_REQ:
	case IOCTL_BFREQ_REQ:
		ret = update_sys_frequency(arg);
		break;
	case IOCTL_COMM_REQ:
		ret = update_commons(arg);
		break;
	case IOCTL_PERF_LEVEL_REQ:
		ret = update_sys_perf_level(arg);
		break;
	case IOCTL_BASIC_TUNABLE_REQ:
		ret = update_sys_tunables(arg);
		break;
#ifdef BMC
	case IOCTL_TUNALBE_BMC_REQ:
		update_sys_opt_tunables(arg);
		break;
#endif
	case IOCTL_PING_REQ:
		ping_expired = 0;
		ping_time = ktime_to_ms(ktime_get());
		break;
#ifdef CURR_HIST
	case IOCTL_LOAD_REQ:
		update_sys_cpu_load(arg);
		break;
#endif
	default:
		break;
	}
	return ret;
}
#ifdef CONFIG_COMPAT
static long compat_io_progress(struct file *filep,
		unsigned int cmd, unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);
	return io_progress(filep, cmd, arg);
}
#endif
static int io_release(struct inode *node, struct file *filep)
{
	return 0;
}

static int io_open(struct inode *node, struct file *filep)
{
	int ret = 0;
	struct io_dev *dev;
	dev = container_of(node->i_cdev, struct io_dev,
			char_dev);
	filep->private_data = dev;
	return ret;
}
static int cpu_stat_callback(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;

	dev = get_cpu_device(cpu);
	if (dev) {
		switch (action & ~CPU_TASKS_FROZEN) {
		case CPU_DOWN_PREPARE:
		case CPU_ONLINE:
#ifndef CORE_CONTROL
			stop_kpolicy();
#endif
			break;
		default:
			break;
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block __refdata cpu_stat_notifier = {
	.notifier_call = cpu_stat_callback,
};

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = io_open,
	.unlocked_ioctl = io_progress,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_io_progress,
#endif
	.release = io_release,
};
static int ioctl_init(void)
{
	int ret = 0;
	dev_t	io_dev_main;
	struct device *io_device;

	ret = alloc_chrdev_region(&io_dev_main, 0, 1, IOCTL_PATH);
	if(ret < 0) {
		pr_err("error in allocation device region\n");
		goto ioctl_init_exit;
	}
	platform_data.major = MAJOR(io_dev_main);
	platform_data.class = class_create(THIS_MODULE, "class_triton");

	if(IS_ERR(platform_data.class)) {
		pr_err("error in creating class triton\n");
		ret = PTR_ERR(platform_data.class);
		goto ioctl_class_fail;
	}

	io_device = device_create(platform_data.class,
				              NULL,
				              io_dev_main,
				              NULL,
				              IOCTL_PATH);
	if(IS_ERR(io_device)) {
		pr_err("error in creating triton device\n");
		ret = PTR_ERR(io_device);
		goto ioctl_dev_fail;
	}
	platform_data.tio_dev = kmalloc(sizeof(struct io_dev), GFP_KERNEL);
	if(!platform_data.tio_dev) {
		pr_err("error in allocation memory\n");
		ret = -ENOMEM;
		goto ioctl_clean_all;
	}
	memset(platform_data.tio_dev, 0, sizeof(struct io_dev));
	sema_init(&platform_data.tio_dev->sem, 1);
	cdev_init(&platform_data.tio_dev->char_dev, &fops);

	ret = cdev_add(&platform_data.tio_dev->char_dev, io_dev_main, 1);
	if(ret < 0) {
		pr_err("Error in adding character device\n");
		goto ioctl_clean_all;
	}
	return ret;
ioctl_clean_all:
	device_destroy(platform_data.class, io_dev_main);
ioctl_dev_fail:
	class_destroy(platform_data.class);
ioctl_class_fail:
	unregister_chrdev_region(io_dev_main, 1);
ioctl_init_exit:
	return ret;

}
static void cleanup_device(void)
{
	dev_t dev = MKDEV(platform_data.major, 0);

	if( !platform_data.tio_dev)
		return;
	device_destroy(platform_data.class, dev);
	class_destroy(platform_data.class);
	unregister_chrdev_region(dev, 1);
	kfree(platform_data.tio_dev);
}

static int  __init init(void)
{
	int ret = 0;
	int i;
	struct governor_policy_info *pcpu;
#if (NUM_CLUSTER > 1)
	int csiblings[MAX_CPUS_DEFLT] = {-1,};
	for (i = 0; i < MAX_CORES; i++) {
		csiblings[i]= topology_physical_package_id(i);
	}
	for (i = 0; i < MAX_CORES; i++) {
		if (csiblings[i] == 0)
			lcluster_cores++;
		else if (csiblings[i] == 1)
			bcluster_cores++;
	}
	lcluster_start = 0;
	bcluster_start = lcluster_cores;
#endif
	for_each_possible_cpu(i) {
		pcpu = &per_cpu(policy_info, i);
		init_rwsem(&pcpu->sem);
	}
	platform_data.fwq = alloc_workqueue("t:fwq", WQ_HIGHPRI, 0);
	platform_data.ping_fwq = alloc_workqueue("t:pfwq", WQ_HIGHPRI, 0);
	INIT_DELAYED_WORK(&platform_data.frequency_changed_wq, frequency_process);
	INIT_DELAYED_WORK(&platform_data.ping_wq, ping_process);
	INIT_WORK(&platform_data.sysfs_wq, sysfs_noti_process);
	spin_lock_init(&platform_data.hotplug_lock);
	spin_lock_init(&platform_data.frequency_change_lock);
	create_sysfs();
	ioctl_init();
	register_hotcpu_notifier(&cpu_stat_notifier);

	platform_data.state = FREEZE;
	platform_data.level = 0;
	platform_data.notify_info.cur_policy = 0;
	queue_delayed_work(platform_data.ping_fwq,
				&platform_data.ping_wq,
				msecs_to_jiffies(DEFAULT_PING_INTV * 5));
	return ret;
}
static void __exit texit(void)
{
	cleanup_device();
	destroy_workqueue(platform_data.fwq);
	unregister_hotcpu_notifier(&cpu_stat_notifier);
}
/*=====================================================================
 * debug fs
 =======================================================================*/
#define show_one(file_name, object)                                     \
	static ssize_t show_##file_name                                 \
	(struct kobject *kobj, struct attribute *attr, char *buf)       \
	{                                                               \
		return sprintf(buf, "%u\n", platform_data.notify_info.object);               \
	}

static ssize_t store_enable(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	platform_data.notify_info.enable = input;
	sysfs_set_noti_data(SYSFS_NOTIFY_ENABLE);
	return count;
}
static ssize_t store_enforce(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	platform_data.notify_info.enforce = input;
	sysfs_set_noti_data(SYSFS_NOTIFY_ENFORCE);
	return count;
}
static ssize_t store_aevents(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	return count;
}
static ssize_t store_bevents(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	return count;
}


static ssize_t store_cur_policy(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	platform_data.notify_info.cur_policy = input;
	sysfs_set_noti_data(SYSFS_NOTIFY_CUR_POLICY);
	return count;
}
static ssize_t store_debug(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	platform_data.notify_info.debug = input;
	sysfs_set_noti_data(SYSFS_NOTIFY_DEBUG);
	return count;
}


show_one(cur_policy, cur_policy);
show_one(enable, enable);
show_one(enforce, enforce);
show_one(aevents, aevents);
show_one(bevents, bevents);
show_one(debug, debug);

define_one_global_rw(cur_policy);
define_one_global_rw(enable);
define_one_global_rw(enforce);
define_one_global_rw(aevents);
define_one_global_rw(bevents);
define_one_global_rw(debug);

static struct attribute *_attributes[] = {
	&enable.attr,
	&aevents.attr,
	&bevents.attr,
	&cur_policy.attr,
	&enforce.attr,
	&debug.attr,
	NULL
};

static struct attribute_group attr_group = {
	.attrs = _attributes,
};
static void create_sysfs(void)
{
	int rc;

	platform_data.kobject = kobject_create_and_add("triton",
			&cpu_subsys.dev_root->kobj);
	rc = sysfs_create_group(platform_data.kobject,
		&attr_group);
	BUG_ON(rc);
}

fs_initcall(init);
module_exit(texit);


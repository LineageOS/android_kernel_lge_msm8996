#include "triton.h"

static int lcluster_cores;
static int bcluster_cores;
static int lcluster_start;
static int bcluster_start;
static u64 ping_time, last_ping_time;
#define DEFAULT_EXPIRED	(10000)
#define DEFAULT_PING_INTV (5000) /* 1 sec */
static int ping_expired = 0;
static struct config conf;
static DEFINE_PER_CPU(struct cpuinfo, cpu_info);
static void create_fs(void);
extern int 	cpufreq_interactive_governor_stat(int cpu);
extern struct cpufreq_policy *cpufreq_interactive_get_policy(int cpu);
static 	int 	get_dst_cpu(int pol, int *cl);

#define GET_CL_PER_CPU(c)  ((c >=0) ? ((c < lcluster_cores) ? 0 : 1) : -1)
#define GET_CL_PER_POL(p)	 ((p > NT) ? ((p < SSTB) ? 0 : 1) : -1)
#define FIRST_CPU_PER_CL(c) 	(c << (CORES_PER_CLUSTER >>1))
#define LAST_CPU_PER_CL(c) 	(CORES_PER_CLUSTER << c)

/*---------------------------------------------
  * state machine
  *--------------------------------------------*/

static int start_kpolicy(int kpolicy)
{
	unsigned long flag;
	struct sys_info *sys = &conf.sys;

	spin_lock_irqsave(&conf.hotplug_lock, flag);
	sys->cur_policy = kpolicy;

	if(sys->sm == FREEZE && kpolicy > NT) {
		sys->sm = RUNNING;
		sys->sw = 1; /* CEILING */
		queue_delayed_work(conf.fwq,
					&conf.frequency_changed_wq,
					usecs_to_jiffies(conf.tunables.tunable_timer_rate_us));

		conf.sysfs_id = SYSFS_CUR_PL;
		schedule_work(&conf.sysfs_wq);
	}
	sys->lsp = sys->cur_policy;
	spin_unlock_irqrestore(&conf.hotplug_lock, flag);
	return 0;
}


static int stop_kpolicy(int urgent)
{
	struct sys_info *sys = &conf.sys;
	int temp_last_policy = 0, cl = -1, dest_cpu = -1;
	struct device *dev;
	if(sys->cur_policy == 0 || sys->sm == FREEZE)
		return 0;
	temp_last_policy = sys->cur_policy;
	sys->cur_policy = 0;
	sys->sm = FREEZE;
	cancel_delayed_work(&conf.frequency_changed_wq);

	/* confirmed stop */
	conf.sysfs_id = SYSFS_CUR_PL;
	schedule_work(&conf.sysfs_wq);

	/* restore frequency of last policy */
	dest_cpu = get_dst_cpu(temp_last_policy, &cl);
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
	struct cpuinfo *pcpu = &per_cpu(cpu_info, cpu);

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
		cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);
		return 0;
	}
noti:
	return -EINVAL;
}

int get_tstate(int cpu)
{
        int cl = GET_CL_PER_CPU(cpu);
        int ret = 1;
	int policy = conf.sys.cur_policy;

	if(!policy || cl < 0)
		return ret;

	switch(policy) {
	case SSTB:
		ret = !cl;
		break;
	case STB:
	case NOM:
		ret = cl;
		break;
	default:
		break;
        }
        return ret;
}

static int get_dst_cpu(int pol, int *cl)
{
	int i, first_cpu, last_cpu;
	struct device *dev;

	*cl = GET_CL_PER_POL(pol);
	if(*cl < 0)
		return -1;
	last_cpu = LAST_CPU_PER_CL(*cl);
	first_cpu = FIRST_CPU_PER_CL(*cl);
	for(i = first_cpu; i < last_cpu ; i++) {
		dev = get_cpu_device(i);
		if(dev && !cpu_is_offline(dev->id)) {
			return i;
		}
	}
	return -1;
}
#ifdef BMC

static int get_opt_frequency(int cl, int req_freq)
{
	int i, last_cpu, first_cpu, freq;
	unsigned long loadtmp;
	unsigned int load[CORES_PER_CLUSTER], loadfreq;
	unsigned int diff, coeff;


	last_cpu = LAST_CPU_PER_CL(cl);
	first_cpu = FIRST_CPU_PER_CL(cl);

	freq = conf.freq_tx[cl].freq;
	coeff = conf.tunables_opt[cl].coeff_b;

	for(i = first_cpu; i < last_cpu; i++) {
		if(cpu_is_offline(i)){
			pr_debug("cpu%d offlined \n", i);
			return -1;
		}
		loadtmp = (u64)sched_get_busy(i) * coeff;
		load[i-first_cpu] = (unsigned int)loadtmp;
	}

	last_cpu = last_cpu-1;
	if ( load[first_cpu-first_cpu] >= load[last_cpu-first_cpu] ){
		diff = load[first_cpu-first_cpu] - load[last_cpu-first_cpu];
		loadfreq = load[first_cpu-first_cpu];
	} else {
		diff = load[last_cpu-first_cpu] - load[first_cpu-first_cpu];
		loadfreq = load[last_cpu-first_cpu];
	}
	if (conf.freq_tx[cl].freq <= conf.tunables_opt[cl].thres_0_b) {
		return freq;
	}

	if (cl) {
		if (freq >= conf.tunables_opt[cl].thres_2_b) {
			if (freq >= conf.tunables_opt[cl].thres_3_b) {
				if (diff >= conf.tunables_opt[cl].dr_1&&
					diff < conf.tunables_opt[cl].dr_2) {
					freq = conf.tunables_opt[cl].opt_1_b;
				} else if (diff >=conf.tunables_opt[cl].dr_2) {
					freq = conf.tunables_opt[cl].opt_2_b;
				}
			} else {
				if (diff >= conf.tunables_opt[cl].dr_1 &&
					diff <  conf.tunables_opt[cl].dr_2) {
					freq = conf.tunables_opt[cl].opt_3_b;
				} else if (diff >= conf.tunables_opt[cl].dr_2) {
					freq = conf.tunables_opt[cl].opt_4_b;
				}
			}
		} else {
			if (diff >= conf.tunables_opt[cl].dr_1 &&
				diff < conf.tunables_opt[cl].dr_2) {
				freq = conf.tunables_opt[cl].opt_5_b;
			} else if (diff >= conf.tunables_opt[cl].dr_2) {
				freq = conf.tunables_opt[cl].opt_6_b;
			}
		}
	}else {
		if (diff >= conf.tunables_opt[cl].dr_1 &&
			diff < conf.tunables_opt[cl].dr_2) {
			freq = conf.tunables_opt[cl].opt_1_b;
		} else if (diff >= conf.tunables_opt[cl].dr_2) {
			freq = conf.tunables_opt[cl].opt_2_b;
		}
	}
	if(!freq)
		freq = req_freq;
	return freq;
}
#endif

/*
 * select the optimal frequency based on the gap of load among cores within cluster
 */
static int get_dst_freq(int dest_cpu, int req_freq)
{
	int cl = -1;

	struct device *dev;

	cl = GET_CL_PER_CPU(dest_cpu);
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
	return get_opt_frequency(cl, req_freq);
#else
	return req_freq;
#endif
}
static void sysfs_noti_process(struct work_struct *sysfs_noti_work)
{
	struct config *conf = container_of(sysfs_noti_work,
							struct config,
							sysfs_wq);
	switch(conf->sysfs_id) {
	case SYSFS_CUR_PL:
		sysfs_notify(conf->kobject, NULL, "cur_policy");
		break;
	case SYSFS_AFREQ:
		sysfs_notify(conf->kobject, NULL, "aevents");
		break;
	case SYSFS_BFREQ:
		sysfs_notify(conf->kobject, NULL, "bevents");
		break;
	case SYSFS_EN:
		sysfs_notify(conf->kobject, NULL, "enable");
		break;
#ifdef TR_DEBUG
	case SYSFS_ENF:
		sysfs_notify(conf->kobject, NULL, "enforce");
		break;
#endif		
	case SYSFS_DBG:
		sysfs_notify(conf->kobject, NULL, "debug");
		break;
	}
}
static void sysfs_set_noti_data(int data)
{
	conf.sysfs_id = data;
	schedule_work(&conf.sysfs_wq);
}
static void ping_process(struct work_struct *work)
{
	last_ping_time = ktime_to_ms(ktime_get());
	if(last_ping_time - ping_time >= DEFAULT_EXPIRED) {
		ping_time = last_ping_time;
		ping_expired = 1;
		stop_kpolicy(1);
	}
	queue_delayed_work(conf.ping_fwq,
					&conf.ping_wq,
					msecs_to_jiffies(DEFAULT_PING_INTV));
}
static void frequency_process(struct work_struct *work)
{
	struct config *conf = container_of(work, struct config,
				frequency_changed_wq.work);
	int dst_cpu = -1, cl = -1;
	int f = 0;
	int ccm = 0;
#ifdef CONFIG_LGE_PM_CANCUN
	ccm = get_cancun_status();
#endif
	if(!conf)
		return;

	if(!conf->sys.enable)
		goto exit;
	if(conf->sys.sm == RUNNING) {
		dst_cpu = get_dst_cpu(conf->sys.cur_policy, &cl);
		if(dst_cpu < 0) {
			pr_err("dst cpu(%d) is not valid\n", dst_cpu);
			goto exit;
		}
		if(!conf->sys.sw && cl >= 0) {
			/* CEILING */
			f = get_dst_freq(dst_cpu, conf->param[cl].turbo);
			if(f < 0)
				goto exit;
			if(f <= conf->param[cl].perf)
				f = conf->param[cl].perf;
			if(!set_dst_cpu(f, dst_cpu)) {
				conf->sys.sw ^= 1;
			} else {
				goto exit;
			}
		}else if(conf->sys.sw) {
			if(ccm) {
				f = conf->param[cl].perf;
			} else {
				f = get_dst_freq(dst_cpu, conf->param[cl].effi);
				if(f < 0)
					goto exit;
				f = f -  conf->tunables_opt[cl].dr_3;
				if( f <= conf->param[cl].perf)
					f = conf->param[cl].perf;
			}
			if(!set_dst_cpu(f, dst_cpu)) {
				conf->sys.sw ^= 1;
			} else {
				goto exit;
			}
		}
		queue_delayed_work(conf->fwq,
					&conf->frequency_changed_wq,
					usecs_to_jiffies(conf->tunables.tunable_timer_rate_us));

	} else {
		goto exit;
	}

	return;
exit:
	stop_kpolicy(0);
	return;
}
static long validate(unsigned int *cmd, unsigned long *arg)
{
	long ret = 0, err_val = 0;
	if((_IOC_TYPE(*cmd) != MAGIC_NUM)) {
		return -EFAULT;
	}
	if(_IOC_NR(*cmd) >= E_MAX) {
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
	int value = -1;
	ret = copy_from_user(&conf.comm, argp,
			sizeof(struct comm));
	if(ret)
		return -ENOTTY;
	value = conf.comm.val;

	switch(conf.comm.req) {
		/* policy */
	case COMM_POL:
		if(value == 0) {
			stop_kpolicy(0);
			break;
		}
		start_kpolicy(value);
		break;
	/* enable */
	case COMM_ENA:
		conf.sys.enable = value;
		break;
#ifdef TR_DEBUG
	/* enforce */
	case COMM_ENF:
		conf.sys.enforce = value;
		break;
#endif		
	default:
		break;
	}
	return ret;
}
void stack(int cpu, int freq)
{
	unsigned long flag = 0;
	int cluster = -1;
	spin_lock_irqsave(&conf.frequency_change_lock, flag);
	cluster = GET_CL_PER_CPU(cpu);
	if(cluster < 0) {
		pr_err("cluster is not valid..... -1\n");
		spin_unlock_irqrestore(&conf.frequency_change_lock, flag);
		return;
	}
	conf.freq_tx[cluster].cluster = cluster;
	conf.freq_tx[cluster].cpu = cpu;
	conf.freq_tx[cluster].freq = freq;

	spin_unlock_irqrestore(&conf.frequency_change_lock, flag);

	switch(cluster) {
	case 0:
		sysfs_notify(conf.kobject, NULL, "aevents");
		break;
	case 1:
		sysfs_notify(conf.kobject, NULL, "bevents");
		break;
	default:
		break;
	}
}
static int update_sys_frequency
		(unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;
	struct freq_info     info;
	ret = copy_from_user(&info, argp,
				sizeof(struct freq_info));

	if(ret)
		return -ENOTTY;

	ret = copy_to_user((void __user*)arg, &conf.freq_tx[info.request_cl],
		sizeof(struct freq_info));

	return ret;
}
void update_cpu_load(int cpu)
{
	struct cpuinfo *pcpu = &per_cpu(cpu_info, cpu);
	int cl = GET_CL_PER_CPU(cpu);
	if(cl < 0 || ping_expired)
		return;
	pcpu->prev_load+= sched_get_busy(cpu);
	pcpu->prev_load_cnt++;
}

static int update_sys_cpu_load(unsigned long *arg)
{
	struct load_info load[MAX_CORES] ;
	struct cpuinfo *pcpu;
	int ret = 0, i;

	for_each_online_cpu(i) {
		pcpu = &per_cpu(cpu_info, i);
		load[i].prev_load_cnt = pcpu->prev_load_cnt;
		load[i].prev_load = pcpu->prev_load;
		pcpu->prev_load_cnt = 0;
		pcpu->prev_load = 0;
	}
	ret = copy_to_user((void __user*)(*arg), load,
		sizeof(struct load_info) * MAX_CORES);

	return ret;
}

static int update_sys_tunables(unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;
	ret = copy_from_user(&conf.tunables, argp,
			sizeof(struct tunables));
	if(ret)
		return -ENOTTY;
	return ret;
}
#ifdef BMC
static int update_sys_opt_tunables(unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;
	ret = copy_from_user(&conf.tunables_opt, argp,
			sizeof(struct tunables_opts) * NUM_CLUSTER);
	if(ret)
		return -ENOTTY;

	pr_info("< tunables %d %d %d %d %d %d %d %d %d %d %d %d %d %d>\n",
		conf.tunables_opt[0].coeff_b ,
		conf.tunables_opt[0].thres_0_b ,
		conf.tunables_opt[0].thres_1_b ,
		conf.tunables_opt[0].thres_2_b ,
		conf.tunables_opt[0].thres_3_b ,
		conf.tunables_opt[0].dr_1 ,
		conf.tunables_opt[0].dr_2 ,
		conf.tunables_opt[0].dr_3 ,
		conf.tunables_opt[0].opt_1_b ,
		conf.tunables_opt[0].opt_2_b ,
		conf.tunables_opt[0].opt_3_b ,
		conf.tunables_opt[0].opt_4_b ,
		conf.tunables_opt[0].opt_5_b ,
		conf.tunables_opt[0].opt_6_b );
	pr_info("< tunables %d %d %d %d %d %d %d %d %d %d %d %d %d %d>\n",
		conf.tunables_opt[1].coeff_b ,
		conf.tunables_opt[1].thres_0_b ,
		conf.tunables_opt[1].thres_1_b ,
		conf.tunables_opt[1].thres_2_b ,
		conf.tunables_opt[1].thres_3_b ,
		conf.tunables_opt[1].dr_1 ,
		conf.tunables_opt[1].dr_2 ,
		conf.tunables_opt[1].dr_3 ,
		conf.tunables_opt[1].opt_1_b ,
		conf.tunables_opt[1].opt_2_b ,
		conf.tunables_opt[1].opt_3_b ,
		conf.tunables_opt[1].opt_4_b ,
		conf.tunables_opt[1].opt_5_b ,
		conf.tunables_opt[1].opt_6_b );
	return ret;

}
#endif
static int update_sys_perf_level(unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;
	ret = copy_from_user(&conf.param, argp,
			sizeof(struct perf_level) * BIT_MAX);
	if(ret)
		return -ENOTTY;

	return ret;
}

static int update_sys_cluster_info(unsigned long *arg)
{
	struct cluster_info cluster;
	int ret;
	cluster.total = MAX_CORES;
	cluster.num_cluster = NUM_CLUSTER;
	cluster.little_min = lcluster_start;
	cluster.big_min = bcluster_start;

	ret = copy_to_user((void __user*)(*arg), &cluster,
		sizeof(struct cluster_info));

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
	case IO_R_CPU_AFREQ:
	case IO_R_CPU_BFREQ:
		ret = update_sys_frequency(arg);
		break;
	case IO_R_CPU_LOAD:
		ret = update_sys_cpu_load(&arg);
		break;
	case IO_R_CPU_INFO:
		ret = update_sys_cluster_info(&arg);
		break;
	case IO_W_COMMON:
		ret = update_commons(arg);
		break;
	case IO_W_CPU_PERF_LEVEL:
		ret = update_sys_perf_level(arg);
		break;
	case IO_W_CPU_TUNABLES:
		ret = update_sys_tunables(arg);
		break;
#ifdef BMC
	case IO_W_CPU_TUNABLES_OPT:
		update_sys_opt_tunables(arg);
		break;
#endif
	case IO_W_PING:
		ping_expired = 0;
		ping_time = ktime_to_ms(ktime_get());
		break;
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
			stop_kpolicy(1);
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
	conf.major = MAJOR(io_dev_main);
	conf.class = class_create(THIS_MODULE, "class_triton");

	if(IS_ERR(conf.class)) {
		pr_err("error in creating class triton\n");
		ret = PTR_ERR(conf.class);
		goto ioctl_class_fail;
	}

	io_device = device_create(conf.class, NULL, io_dev_main, NULL,
				IOCTL_PATH);
	if(IS_ERR(io_device)) {
		pr_err("error in creating triton device\n");
		ret = PTR_ERR(io_device);
		goto ioctl_dev_fail;
	}
	conf.tio_dev = kmalloc(sizeof(struct io_dev), GFP_KERNEL);
	if(!conf.tio_dev) {
		pr_err("error in allocation memory\n");
		ret = -ENOMEM;
		goto ioctl_clean_all;
	}
	memset(conf.tio_dev, 0, sizeof(struct io_dev));
	sema_init(&conf.tio_dev->sem, 1);
	cdev_init(&conf.tio_dev->char_dev, &fops);

	ret = cdev_add(&conf.tio_dev->char_dev, io_dev_main, 1);
	if(ret < 0) {
		pr_err("Error in adding character device\n");
		goto ioctl_clean_all;
	}
	return ret;
ioctl_clean_all:
	device_destroy(conf.class, io_dev_main);
ioctl_dev_fail:
	class_destroy(conf.class);
ioctl_class_fail:
	unregister_chrdev_region(io_dev_main, 1);
ioctl_init_exit:
	return ret;

}
static void cleanup_device(void)
{
	dev_t dev = MKDEV(conf.major, 0);

	if( !conf.tio_dev)
		return;
	device_destroy(conf.class, dev);
	class_destroy(conf.class);
	unregister_chrdev_region(dev, 1);
	kfree(conf.tio_dev);
}

static int  __init init(void)
{
	int ret = 0;
	int i;
	int csiblings[MAX_CPUS_DEFLT] = {-1,};
	struct cpuinfo *pcpu;
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
	for_each_possible_cpu(i) {
		pcpu = &per_cpu(cpu_info, i);
		init_rwsem(&pcpu->sem);
	}
	conf.fwq = alloc_workqueue("t:fwq", WQ_HIGHPRI, 0);
	conf.ping_fwq = alloc_workqueue("t:pfwq", WQ_HIGHPRI, 0);
	INIT_DELAYED_WORK(&conf.frequency_changed_wq, frequency_process);
	INIT_DELAYED_WORK(&conf.ping_wq, ping_process);
	INIT_WORK(&conf.sysfs_wq, sysfs_noti_process);
	spin_lock_init(&conf.hotplug_lock);
	spin_lock_init(&conf.frequency_change_lock);
	create_fs();
	ioctl_init();
	register_hotcpu_notifier(&cpu_stat_notifier);

	conf.sys.sm = FREEZE;
	conf.sys.cur_policy = 0;
	conf.sys.lsp = 0;
	conf.sys.sw = 0;
	queue_delayed_work(conf.ping_fwq,
				&conf.ping_wq,
				msecs_to_jiffies(DEFAULT_PING_INTV * 5));
	return ret;
}
static void __exit texit(void)
{
	cleanup_device();
	destroy_workqueue(conf.fwq);
	unregister_hotcpu_notifier(&cpu_stat_notifier);
}
/*=====================================================================
 * debug fs
 =======================================================================*/
#define show_one(file_name, object)                                     \
	static ssize_t show_##file_name                                 \
	(struct kobject *kobj, struct attribute *attr, char *buf)       \
	{                                                               \
		return sprintf(buf, "%u\n", conf.sys.object);               \
	}
static ssize_t store_enable(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	conf.sys.enable = input;
	conf.init_ok = true;
	sysfs_set_noti_data(SYSFS_EN);
	return count;
}
#ifdef TR_DEBUG
static ssize_t store_enforce(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	conf.sys.enforce = input;
	sysfs_set_noti_data(SYSFS_ENF);
	return count;
}
#endif
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
	conf.sys.cur_policy = input;
	sysfs_set_noti_data(SYSFS_CUR_PL);
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
	conf.sys.debug = input;
	sysfs_set_noti_data(SYSFS_DBG);
	return count;
}

show_one(cur_policy, cur_policy);
show_one(enable, enable);
#ifdef TR_DEBUG
show_one(enforce, enforce);
#endif
show_one(aevents, aevents);
show_one(bevents, bevents);
show_one(debug, debug);

define_one_global_rw(cur_policy);
define_one_global_rw(enable);
#ifdef TR_DEBUG
define_one_global_rw(enforce);
#endif
define_one_global_rw(aevents);
define_one_global_rw(bevents);
define_one_global_rw(debug);

static struct attribute *_attributes[] = {
	&enable.attr,
	&aevents.attr,
	&bevents.attr,
	&cur_policy.attr,
#ifdef TR_DEBUG
	&enforce.attr,
#endif
	&debug.attr,
	NULL
};

static struct attribute_group attr_group = {
	.attrs = _attributes,
};
static void create_fs(void)
{
	int rc;

	conf.kobject= kobject_create_and_add("triton",
			&cpu_subsys.dev_root->kobj);
	rc = sysfs_create_group(conf.kobject,
		&attr_group);
	BUG_ON(rc);
}

fs_initcall(init);
module_exit(texit);


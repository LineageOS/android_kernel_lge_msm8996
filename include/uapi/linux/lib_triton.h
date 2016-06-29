#ifndef _CPU_FERQ_HELPER_IOCTL_H
#define _CPU_FERQ_HELPER_IOCTL_H
#include <linux/ioctl.h>
#define IOCTL_PATH "triton:io"

/* Feature defines */
#define BMC
#define TR_DEBUG
enum state_machine{
	IDLE,
	RUNNING,
	FREEZE
};
#define COMM_POL (0)
#define COMM_ENA (1)
#define COMM_ENF (2)

enum state_policy {
	NT = 0,
	STB,
	NOM,
	SSTB,
};

struct perf_level {
	int turbo;
	int nominal;
	int svs;
	int effi;
	int perf;
	int freeze;
};
struct tunables {
	int tunable_trigger_rate_ms;
	int tunable_timer_rate_us;
	int tunable_stu_high_res;
	int tunable_sstu_high_res;
};
struct tunables_opts {
	int coeff_b;
	int thres_0_b;
	int thres_1_b;
	int thres_2_b;
	int thres_3_b;
	int dr_1;
	int dr_2;
	int dr_3;
	int opt_1_b;
	int opt_2_b;
	int opt_3_b;
	int opt_4_b;
	int opt_5_b;
	int opt_6_b;
};
struct load_info {
	int size;
	unsigned long prev_load;
	int prev_load_cnt;
	int freq;
};

struct freq_info {
	int request_cl;
	int cluster;
	int cpu;
	int freq;
};
struct cluster_info {
	int size;
	int total;
	int num_cluster;
	int little_min;
	int big_min;
};
struct comm {
	int req;
	int val;
};

enum {
	E_CPU_AFREQ = 0,
	E_CPU_BFREQ = 0x01,
	E_CPU_LOAD	= 0x02,
	E_CPU_INFO  = 0x03,
	E_COMMON = 0x05,
	E_CPU_PERF_LEVEL = 0x06,
	E_CPU_TUNABLES = 0x07,
	E_CPU_FREQ_ADJ = 0x09,
#ifdef BMC
	E_CPU_TUNABLES_OPT = 0x0b,
#endif
	E_PING			= 0x0c,
	E_MAX
};
#define BIT_MAX	(3)
#define BIT_GPU		(2)
#define BIT_BIG		(1)
#define BIT_LITTLE 	(0)
#define MAX_CORES          (4)
#define NUM_CLUSTER	(2)
#define CORES_PER_CLUSTER (MAX_CORES/NUM_CLUSTER)
#define MAGIC_NUM	(0x10)
#define IO_R_CPU_AFREQ 		_IOR(MAGIC_NUM, E_CPU_AFREQ, struct freq_info)
#define IO_R_CPU_BFREQ 		_IOR(MAGIC_NUM, E_CPU_BFREQ, struct freq_info)
#define IO_R_CPU_LOAD 		_IOR(MAGIC_NUM, E_CPU_LOAD, struct load_info)
#define IO_R_CPU_INFO 		_IOR(MAGIC_NUM, E_CPU_INFO, struct cluster_info)
#define IO_W_COMMON 		_IOW(MAGIC_NUM, E_COMMON, struct comm)
#define IO_W_CPU_PERF_LEVEL 	_IOW(MAGIC_NUM, E_CPU_PERF_LEVEL, struct perf_level)
#define IO_W_CPU_TUNABLES 	_IOW(MAGIC_NUM, E_CPU_TUNABLES, struct tunables)
#define IO_W_CPU_FREQ	_IOW(MAGIC_NUM, E_CPU_FREQ_ADJ, struct freq_info)
#ifdef BMC
#define IO_W_CPU_TUNABLES_OPT 	_IOW(MAGIC_NUM, E_CPU_TUNABLES_OPT, struct tunables_opts)
#endif
#define IO_W_PING 	_IOW(MAGIC_NUM, E_PING, int)
#endif

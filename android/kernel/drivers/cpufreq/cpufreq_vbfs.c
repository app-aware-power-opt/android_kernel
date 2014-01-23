/*
 *  drivers/cpufreq/cpufreq_vbfs.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *            (C)  KAIST
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>

#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/swap.h>

/*
 * vbfs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */

#define DEF_FREQUENCY_DOWN_DIFFERENTIAL		(10)
#define DEF_FREQUENCY_UP_THRESHOLD		(80)
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MAX_SAMPLING_DOWN_FACTOR		(100000)
#define MICRO_FREQUENCY_DOWN_DIFFERENTIAL	(3)
#define MICRO_FREQUENCY_UP_THRESHOLD		(95)
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE		(10000)
#define MIN_FREQUENCY_UP_THRESHOLD		(11)
#define MAX_FREQUENCY_UP_THRESHOLD		(100)

#define CPUUSAGE_UP_THRESHOLD (40)
#define CPUUSAGE_MIN_UP_THRESHOLD (50)
#define CPUUSAGE_MIN_DOWN_THRESHOLD (30)
#define CPUUSAGE_DOWN_THRESHOLD (-40)
//#define CPUUSAGE_DOWN_THRESHOLD_LIMIT (30)
#define THREAD_UP_THRESHOLD (2)
#define THREAD_DOWN_THRESHOLD (-2)
#define MEM_UP_THRESHOLD (20)
#define MEM_DOWN_THRESHOLD (-20)
#define MAX_VARIATION_THRESHOLD (100)
#define MIN_VARIATION_UP_THRESHOLD (1)
#define MIN_VARIATION_DOWN_THRESHOLD (-100)

/*
 * The polling frequency of this governor depends on the capability of
 * the processor. Default polling frequency is 1000 times the transition
 * latency of the processor. The governor will work on any processor with
 * transition latency <= 10mS, using appropriate sampling
 * rate.
 * For CPUs with transition latency > 10mS (mostly drivers with CPUFREQ_ETERNAL)
 * this governor will not work.
 * All times here are in uS.
 */
#define MIN_SAMPLING_RATE_RATIO			(2)

static unsigned int min_sampling_rate;

#define LATENCY_MULTIPLIER			(1000)
#define MIN_LATENCY_MULTIPLIER			(100)
#define TRANSITION_LATENCY_LIMIT		(10 * 1000 * 1000)

static void do_vbfs_timer(struct work_struct *work);
static int cpufreq_governor_vbfs(struct cpufreq_policy *policy,
				unsigned int event);

static DEFINE_MUTEX(vbfs_mutex);

static int l_vbfs_cpufreq_set_flag = 0;

static int cpufreq_set(struct cpufreq_policy *policy, unsigned int freq)
{
	int ret = -EINVAL;

	l_vbfs_cpufreq_set_flag = 1;

#ifdef CONFIG_CPU_FREQ_DBG
	pr_info("cpufreq_set for cpu %u, freq %u kHz\n", policy->cpu, freq);
#else
	pr_debug("cpufreq_set for cpu %u, freq %u kHz\n", policy->cpu, freq);
#endif

	mutex_lock(&vbfs_mutex);
/*
	if (!per_cpu(cpu_is_managed, policy->cpu))
		goto err;

	per_cpu(cpu_set_freq, policy->cpu) = freq;

	if (freq < per_cpu(cpu_min_freq, policy->cpu))
		freq = per_cpu(cpu_min_freq, policy->cpu);
	if (freq > per_cpu(cpu_max_freq, policy->cpu))
		freq = per_cpu(cpu_max_freq, policy->cpu);
*/
	if(freq < policy->min)
		freq = policy->min;
	if(freq > policy->max)
		freq = policy->max;

	/*
	 * We're safe from concurrent calls to ->target() here
	 * as we hold the userspace_mutex lock. If we were calling
	 * cpufreq_driver_target, a deadlock situation might occur:
	 * A: cpufreq_set (lock userspace_mutex) ->
	 *      cpufreq_driver_target(lock policy->lock)
	 * B: cpufreq_set_policy(lock policy->lock) ->
	 *      __cpufreq_governor ->
	 *         cpufreq_governor_userspace (lock userspace_mutex)
	 */
	ret = __cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);

 err:
	mutex_unlock(&vbfs_mutex);
	return ret;
}


static ssize_t show_speed(struct cpufreq_policy *policy, char *buf)
{
	l_vbfs_cpufreq_set_flag = 0;
	return sprintf(buf, "%u\n", policy->cur);
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_VBFS
static
#endif
struct cpufreq_governor cpufreq_gov_vbfs = {
       .name                   = "vbfs",
       .governor               = cpufreq_governor_vbfs,
       .store_setspeed	= cpufreq_set,
       .show_setspeed	= show_speed,
       .max_transition_latency = TRANSITION_LATENCY_LIMIT,
       .owner                  = THIS_MODULE,
};

/* Sampling types */
enum {VBFS_NORMAL_SAMPLE, VBFS_SUB_SAMPLE};

struct cpu_vbfs_info_s {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_iowait;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_nice;
	struct cpufreq_policy *cur_policy;
	struct delayed_work work;
	struct cpufreq_frequency_table *freq_table;
	unsigned int freq_lo;
	unsigned int freq_lo_jiffies;
	unsigned int freq_hi_jiffies;
	unsigned int rate_mult;
	int cpu;
	unsigned int sample_type:1;
	/*
	 * percpu mutex that serializes governor limit change with
	 * do_vbfs_timer invocation. We do not want do_vbfs_timer to run
	 * when user is changing the governor or limits.
	 */
	struct mutex timer_mutex;
};
static DEFINE_PER_CPU(struct cpu_vbfs_info_s, od_cpu_vbfs_info);

static unsigned int vbfs_enable;	/* number of CPUs using this policy */

/*
 * vbfs_mutex protects vbfs_enable in governor start/stop.
 */
//static DEFINE_MUTEX(vbfs_mutex);

/*
 * adj status
 *  0 : no need to change
 *  1 : settled to min or max frequency
 *  2 : conditions are met (T or M)
 *  3 : step up from the current frequency by CPU utilization
 *  4 : step down from the current frequency by CPU utilization
 *  5 : step up from the current frequency by CPU utilization and conditions (T or M)
 *  6 : step down from the current frequency by CPU utilization and conditions (T or M)
 *  7 : scale up from the given frequency by CPU utilization variation
 *  8 : scale down from the given frequency by CPU utilization variation
 *  9 : scale up from the given frequency  by CPU utilization variation and conditions (T or M)
 * 10 : scale down from the given frequency  by CPU utilization variation and conditions (T or M)
 * 11 : step up from the current frequency by conditions (T or M)
 * 12 : step down from the current frequency by conditions (T or M)
 * 13 : invalid status
 */
enum vbfs_cpufreq_adj {
	ADJ_NO_NEED,		/*0*/
	ADJ_SETTLED,		/*1*/
	ADJ_COND,					/*2*/
	ADJ_STEP_UP,				/*3*/
	ADJ_STEP_DOWN,			/*4*/
	ADJ_STEP_N_COND_UP,		/*5 = ADJ_COND + ADJ_STEP_UP*/
	ADJ_STEP_N_COND_DOWN,	/*6 = ADJ_COND + ADJ_STEP_DOWN*/
	ADJ_SCALE_UP,				/*7*/
	ADJ_SCALE_DOWN,			/*8*/
	ADJ_SCALE_N_COND_UP,		/*9 = ADJ_COND + ADJ_SCALE_UP*/
	ADJ_SCALE_N_COND_DOWN,	/*10 = ADJ_COND + ADJ_SCALE_DOWN*/
	ADJ_COND_UP,		/*11*/
	ADJ_COND_DOWN,	/*12*/
	ADJ_INVALID,		/*13*/
};

static struct vbfs_tuners {
	unsigned int sampling_rate;
	unsigned int cpu_usage_up_threshold;
	unsigned int cpu_usage_down_threshold;
	int cpu_variation_up_threshold;
	int cpu_variation_down_threshold;
	int thread_variation_up_threshold;
	int thread_variation_down_threshold;
	int mem_variation_up_threshold;
	int mem_variation_down_threshold;
	unsigned int ignore_nice;
	unsigned int sampling_down_factor;
	unsigned int powersave_bias;
	unsigned int io_is_busy;
	unsigned int debug;
	unsigned int lazy_freq_step_up_cnt;
	unsigned int lazy_freq_step_down_cnt;
} vbfs_tuners_ins = {
	.cpu_usage_up_threshold = CPUUSAGE_MIN_UP_THRESHOLD,
	.sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR,
	.cpu_usage_down_threshold = CPUUSAGE_MIN_DOWN_THRESHOLD,
	.cpu_variation_up_threshold = CPUUSAGE_UP_THRESHOLD,
	.cpu_variation_down_threshold = CPUUSAGE_DOWN_THRESHOLD,
	.thread_variation_up_threshold = THREAD_UP_THRESHOLD,
	.thread_variation_down_threshold = THREAD_DOWN_THRESHOLD,
	.mem_variation_up_threshold = MEM_UP_THRESHOLD,
	.mem_variation_down_threshold = MEM_DOWN_THRESHOLD,
	.ignore_nice = 0,
	.powersave_bias = 0,
	.debug = 0,
	.lazy_freq_step_up_cnt = 1,
	.lazy_freq_step_down_cnt = 3,
};

static struct vbfs_resources {
	int vbfs_adj;
	unsigned int next_freq;
	unsigned int maxfreqindex;
	unsigned int minfreqindex;
	unsigned int max_load_freq;
	unsigned int max_load;
	unsigned int load_prev;
	int load_diff;
	int freq_avg;
	unsigned int thread_prev;
	int thread_diff;
	unsigned int mem_prev;
	int mem_diff;
	unsigned long long t_init;
	unsigned long long t_prev;
} vbfs_resources_ins = {
	.vbfs_adj = ADJ_NO_NEED,
	.next_freq = 0,
	.maxfreqindex = 0,
	.minfreqindex = 0,
	.max_load_freq = 0,
	.max_load = 0,
	.load_prev = 0,
	.load_diff = 0,
	.freq_avg = 0,
	.thread_prev = 0,
	.thread_diff = 0,
	.mem_prev = 0,
	.mem_diff = 0,
	.t_init = 0,
	.t_prev = 0,
};

static inline cputime64_t get_cpu_idle_time_jiffy(unsigned int cpu,
							cputime64_t *wall)
{
	cputime64_t idle_time;
	cputime64_t cur_wall_time;
	cputime64_t busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());
	busy_time = cputime64_add(kstat_cpu(cpu).cpustat.user,
			kstat_cpu(cpu).cpustat.system);

	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.irq);
	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.softirq);
	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.steal);
	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.nice);

	idle_time = cputime64_sub(cur_wall_time, busy_time);
	if (wall)
		*wall = (cputime64_t)jiffies_to_usecs(cur_wall_time);

	return (cputime64_t)jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, wall);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);

	return idle_time;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

/*
 * Find right freq to be set now with powersave_bias on.
 * Returns the freq_hi to be used right now and will set freq_hi_jiffies,
 * freq_lo, and freq_lo_jiffies in percpu area for averaging freqs.
 */
static unsigned int powersave_bias_target(struct cpufreq_policy *policy,
					  unsigned int freq_next,
					  unsigned int relation)
{
	unsigned int freq_req, freq_reduc, freq_avg;
	unsigned int freq_hi, freq_lo;
	unsigned int index = 0;
	unsigned int jiffies_total, jiffies_hi, jiffies_lo;
	struct cpu_vbfs_info_s *vbfs_info = &per_cpu(od_cpu_vbfs_info,
						   policy->cpu);

	if (!vbfs_info->freq_table) {
		vbfs_info->freq_lo = 0;
		vbfs_info->freq_lo_jiffies = 0;
		return freq_next;
	}

	cpufreq_frequency_table_target(policy, vbfs_info->freq_table, freq_next,
			relation, &index);
	freq_req = vbfs_info->freq_table[index].frequency;
	freq_reduc = freq_req * vbfs_tuners_ins.powersave_bias / 1000;
	freq_avg = freq_req - freq_reduc;

	/* Find freq bounds for freq_avg in freq_table */
	index = 0;
	cpufreq_frequency_table_target(policy, vbfs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_H, &index);
	freq_lo = vbfs_info->freq_table[index].frequency;
	index = 0;
	cpufreq_frequency_table_target(policy, vbfs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_L, &index);
	freq_hi = vbfs_info->freq_table[index].frequency;

	/* Find out how long we have to be in hi and lo freqs */
	if (freq_hi == freq_lo) {
		vbfs_info->freq_lo = 0;
		vbfs_info->freq_lo_jiffies = 0;
		return freq_lo;
	}
	jiffies_total = usecs_to_jiffies(vbfs_tuners_ins.sampling_rate);
	jiffies_hi = (freq_avg - freq_lo) * jiffies_total;
	jiffies_hi += ((freq_hi - freq_lo) / 2);
	jiffies_hi /= (freq_hi - freq_lo);
	jiffies_lo = jiffies_total - jiffies_hi;
	vbfs_info->freq_lo = freq_lo;
	vbfs_info->freq_lo_jiffies = jiffies_lo;
	vbfs_info->freq_hi_jiffies = jiffies_hi;
	return freq_hi;
}

static void vbfs_powersave_bias_init_cpu(int cpu)
{
	struct cpu_vbfs_info_s *vbfs_info = &per_cpu(od_cpu_vbfs_info, cpu);
	vbfs_info->freq_table = cpufreq_frequency_get_table(cpu);
	vbfs_info->freq_lo = 0;
}

static void vbfs_powersave_bias_init(void)
{
	int i;
	for_each_online_cpu(i) {
		vbfs_powersave_bias_init_cpu(i);
	}
}

/************************** sysfs interface ************************/

static ssize_t show_sampling_rate_min(struct kobject *kobj,
				      struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", min_sampling_rate);
}

define_one_global_ro(sampling_rate_min);

/* cpufreq_vbfs Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)              \
{									\
	return sprintf(buf, "%d\n", vbfs_tuners_ins.object);		\
}
#define show_one_float(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)              \
{									\
	return sprintf(buf, "%3.2f\n", vbfs_tuners_ins.object);		\
}
show_one(sampling_rate, sampling_rate);
show_one(io_is_busy, io_is_busy);
show_one(cpu_usage_up_threshold, cpu_usage_up_threshold);
show_one(cpu_usage_down_threshold, cpu_usage_down_threshold);
show_one(cpu_variation_up_threshold, cpu_variation_up_threshold);
show_one(cpu_variation_down_threshold, cpu_variation_down_threshold);
show_one(thread_variation_up_threshold, thread_variation_up_threshold);
show_one(thread_variation_down_threshold, thread_variation_down_threshold);
show_one(mem_variation_up_threshold, mem_variation_up_threshold);
show_one(mem_variation_down_threshold, mem_variation_down_threshold);
show_one(sampling_down_factor, sampling_down_factor);
show_one(ignore_nice_load, ignore_nice);
show_one(powersave_bias, powersave_bias);
show_one(debug, debug);
show_one(lazy_freq_step_up_cnt, lazy_freq_step_up_cnt);
show_one(lazy_freq_step_down_cnt, lazy_freq_step_down_cnt);

static ssize_t store_sampling_rate(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	vbfs_tuners_ins.sampling_rate = max(input, min_sampling_rate);
	return count;
}

static ssize_t store_io_is_busy(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	vbfs_tuners_ins.io_is_busy = !!input;
	return count;
}

static ssize_t store_cpu_usage_up_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	vbfs_tuners_ins.cpu_usage_up_threshold = input;
	pr_info("%s, value : %u", __func__, input);
	return count;
}

static ssize_t store_cpu_usage_down_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > vbfs_tuners_ins.cpu_usage_up_threshold ||
			input < MICRO_FREQUENCY_DOWN_DIFFERENTIAL) {
		return -EINVAL;
	}
	vbfs_tuners_ins.cpu_usage_down_threshold = input;
	pr_info("%s, value : %u", __func__, input);
	return count;
}

static ssize_t store_cpu_variation_up_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1 || input > MAX_VARIATION_THRESHOLD ||
			input < MIN_VARIATION_UP_THRESHOLD) {
		return -EINVAL;
	}
	vbfs_tuners_ins.cpu_variation_up_threshold = input;
	pr_info("%s, value : %d", __func__, input);
	return count;
}

static ssize_t store_cpu_variation_down_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1 || input > vbfs_tuners_ins.cpu_variation_up_threshold ||
			input < MIN_VARIATION_DOWN_THRESHOLD) {
		return -EINVAL;
	}
	vbfs_tuners_ins.cpu_variation_down_threshold = input;
	pr_info("%s, value : %d", __func__, input);
	return count;
}

static ssize_t store_thread_variation_up_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1 || input > MAX_VARIATION_THRESHOLD ||
			input < MIN_VARIATION_UP_THRESHOLD) {
		return -EINVAL;
	}
	vbfs_tuners_ins.thread_variation_up_threshold = input;
	pr_info("%s, value : %d", __func__, input);
	return count;
}

static ssize_t store_thread_variation_down_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1 || input > vbfs_tuners_ins.thread_variation_up_threshold ||
			input < MIN_VARIATION_DOWN_THRESHOLD) {
		return -EINVAL;
	}
	vbfs_tuners_ins.thread_variation_down_threshold = input;
	pr_info("%s, value : %d", __func__, input);
	return count;
}

static ssize_t store_mem_variation_up_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1 || input > MAX_VARIATION_THRESHOLD ||
			input < MIN_VARIATION_UP_THRESHOLD) {
		return -EINVAL;
	}
	vbfs_tuners_ins.mem_variation_up_threshold = input;
	pr_info("%s, value : %d", __func__, input);
	return count;
}

static ssize_t store_mem_variation_down_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1 || input > vbfs_tuners_ins.mem_variation_up_threshold ||
			input < MIN_VARIATION_DOWN_THRESHOLD) {
		return -EINVAL;
	}
	vbfs_tuners_ins.mem_variation_down_threshold = input;
	pr_info("%s, value : %d", __func__, input);
	return count;
}

static ssize_t store_sampling_down_factor(struct kobject *a,
			struct attribute *b, const char *buf, size_t count)
{
	unsigned int input, j;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;
	vbfs_tuners_ins.sampling_down_factor = input;

	/* Reset down sampling multiplier in case it was active */
	for_each_online_cpu(j) {
		struct cpu_vbfs_info_s *vbfs_info;
		vbfs_info = &per_cpu(od_cpu_vbfs_info, j);
		vbfs_info->rate_mult = 1;
	}
	return count;
}

static ssize_t store_ignore_nice_load(struct kobject *a, struct attribute *b,
				      const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == vbfs_tuners_ins.ignore_nice) { /* nothing to do */
		return count;
	}
	vbfs_tuners_ins.ignore_nice = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct cpu_vbfs_info_s *vbfs_info;
		vbfs_info = &per_cpu(od_cpu_vbfs_info, j);
		vbfs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&vbfs_info->prev_cpu_wall);
		if (vbfs_tuners_ins.ignore_nice)
			vbfs_info->prev_cpu_nice = kstat_cpu(j).cpustat.nice;

	}
	return count;
}

static ssize_t store_powersave_bias(struct kobject *a, struct attribute *b,
				    const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > 1000)
		input = 1000;

	vbfs_tuners_ins.powersave_bias = input;
	vbfs_powersave_bias_init();
	return count;
}

static ssize_t store_debug(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	vbfs_tuners_ins.debug = input;
	pr_info("%s, value : %u", __func__, input);
	return count;
}

static unsigned int l_lazy_freq_step_up_cnt = 0;
static unsigned int l_lazy_freq_step_down_cnt = 0;
static ssize_t store_lazy_freq_step_up_cnt(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	vbfs_tuners_ins.lazy_freq_step_up_cnt = input;
	pr_info("%s, value : %u", __func__, input);
	return count;
}

static ssize_t store_lazy_freq_step_down_cnt(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	vbfs_tuners_ins.lazy_freq_step_down_cnt = input;
	pr_info("%s, value : %u", __func__, input);
	return count;
}

define_one_global_rw(sampling_rate);
define_one_global_rw(io_is_busy);
define_one_global_rw(cpu_usage_up_threshold);
define_one_global_rw(cpu_usage_down_threshold);
define_one_global_rw(cpu_variation_up_threshold);
define_one_global_rw(cpu_variation_down_threshold);
define_one_global_rw(thread_variation_up_threshold);
define_one_global_rw(thread_variation_down_threshold);
define_one_global_rw(mem_variation_up_threshold);
define_one_global_rw(mem_variation_down_threshold);
define_one_global_rw(sampling_down_factor);
define_one_global_rw(ignore_nice_load);
define_one_global_rw(powersave_bias);
define_one_global_rw(debug);
define_one_global_rw(lazy_freq_step_up_cnt);
define_one_global_rw(lazy_freq_step_down_cnt);

static struct attribute *vbfs_attributes[] = {
	&sampling_rate_min.attr,
	&sampling_rate.attr,
	&cpu_usage_up_threshold.attr,
	&cpu_usage_down_threshold.attr,
	&cpu_variation_up_threshold.attr,
	&cpu_variation_down_threshold.attr,
	&thread_variation_up_threshold.attr,
	&thread_variation_down_threshold.attr,
	&mem_variation_up_threshold.attr,
	&mem_variation_down_threshold.attr,
	&sampling_down_factor.attr,
	&ignore_nice_load.attr,
	&powersave_bias.attr,
	&io_is_busy.attr,
	&debug.attr,
	&lazy_freq_step_up_cnt.attr,
	&lazy_freq_step_down_cnt.attr,
	NULL
};

static struct attribute_group vbfs_attr_group = {
	.attrs = vbfs_attributes,
	.name = "vbfs",
};

/************************** sysfs end ************************/

extern void status_monitor_write_file(unsigned char* data, unsigned int size, int force);
extern int get_status_monitor_flag(void);

#ifdef CONFIG_CPU_THREAD_NUM
extern int run_thread_number;
#endif

void get_max_load_freq(struct cpufreq_policy *policy)
{
	unsigned int j;
	unsigned int max_load_freq = 0;
	
	for_each_cpu(j, policy->cpus) {
		struct cpu_vbfs_info_s *j_vbfs_info;
		cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load, load_freq;
		int freq_avg;

		j_vbfs_info = &per_cpu(od_cpu_vbfs_info, j);

		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time);
		cur_iowait_time = get_cpu_iowait_time(j, &cur_wall_time);

		wall_time = (unsigned int) cputime64_sub(cur_wall_time,
				j_vbfs_info->prev_cpu_wall);
		j_vbfs_info->prev_cpu_wall = cur_wall_time;

		idle_time = (unsigned int) cputime64_sub(cur_idle_time,
				j_vbfs_info->prev_cpu_idle);
		j_vbfs_info->prev_cpu_idle = cur_idle_time;

		iowait_time = (unsigned int) cputime64_sub(cur_iowait_time,
				j_vbfs_info->prev_cpu_iowait);
		j_vbfs_info->prev_cpu_iowait = cur_iowait_time;

		if (vbfs_tuners_ins.ignore_nice) {
			cputime64_t cur_nice;
			unsigned long cur_nice_jiffies;

			cur_nice = cputime64_sub(kstat_cpu(j).cpustat.nice,
					 j_vbfs_info->prev_cpu_nice);
			/*
			 * Assumption: nice time between sampling periods will
			 * be less than 2^32 jiffies for 32 bit sys
			 */
			cur_nice_jiffies = (unsigned long)
					cputime64_to_jiffies64(cur_nice);

			j_vbfs_info->prev_cpu_nice = kstat_cpu(j).cpustat.nice;
			idle_time += jiffies_to_usecs(cur_nice_jiffies);
		}

		/*
		 * For the purpose of vbfs, waiting for disk IO is an
		 * indication that you're performance critical, and not that
		 * the system is actually idle. So subtract the iowait time
		 * from the cpu idle time.
		 */

		if (vbfs_tuners_ins.io_is_busy && idle_time >= iowait_time)
			idle_time -= iowait_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;

		freq_avg = __cpufreq_driver_getavg(policy, j);
		if (freq_avg <= 0)
			freq_avg = policy->cur;

		load_freq = load * freq_avg;
		if (load_freq > max_load_freq)
		{
			vbfs_resources_ins.max_load_freq = load_freq;
			vbfs_resources_ins.max_load = load;
			vbfs_resources_ins.freq_avg = freq_avg;
		}
	}
}

/*
 * get the total memory usage ratio
 * float is not supported in Kernel, used 100 scaled portion
 */
int get_mem_portion(void)
{
	struct sysinfo i;
	long cached;
	int result = 0;

	si_meminfo(&i);
	si_swapinfo(&i);
	
	cached = global_page_state(NR_FILE_PAGES) -
			total_swapcache_pages - i.bufferram;
	if (cached < 0)
		cached = 0;

	result = (int)(i.totalram - i.freeram - i.bufferram - cached)*10000/i.totalram;

	////pr_info("[DBG] RAM, total : %ld, free : %ld, buffer : %ld, cached : %ld, portion : %d\n", i.totalram, i.freeram, i.bufferram, cached, result);
	return result; 
}

/*
 * select proper frequency from start to end freqency
 * lazy count will be used to prevent too frequent frequency scales
 */
int selectNextFrequencyByLoad(struct cpufreq_policy *policy,  unsigned int cpuUsage, unsigned int cpuFreq, int freq_direction,  unsigned int freq_start,  unsigned int freq_end)
{
	unsigned int freqIndex = 0, freqStartIndex = 0, freqEndIndex = 0, relation = CPUFREQ_RELATION_L;
	int freqadj = 0;
	int direction = freq_direction;
	
	
	struct cpu_vbfs_info_s *vbfs_info = &per_cpu(od_cpu_vbfs_info,
						   policy->cpu);

	if (!vbfs_info->freq_table) {
		vbfs_info->freq_lo = 0;
		vbfs_info->freq_lo_jiffies = 0;
		direction = ADJ_INVALID;
		vbfs_resources_ins.next_freq = 0;
		pr_err("[DBG] invalid freq_table, return\n");
		return direction;
	}

	if((direction == ADJ_STEP_UP) || (direction == ADJ_SCALE_UP)) {
		relation = CPUFREQ_RELATION_H;
	}
	else if((direction == ADJ_STEP_DOWN) || (direction == ADJ_SCALE_DOWN)) {
		relation = CPUFREQ_RELATION_L;
	}
	else {
		direction = ADJ_INVALID;
		vbfs_resources_ins.next_freq = 0;
		pr_err("[DBG] invalid direction, return\n");
		return direction;
	}

	cpufreq_frequency_table_target(policy, vbfs_info->freq_table, cpuFreq, relation, &freqIndex);
	cpufreq_frequency_table_target(policy, vbfs_info->freq_table, freq_start, CPUFREQ_RELATION_L, &freqStartIndex);
	cpufreq_frequency_table_target(policy, vbfs_info->freq_table, freq_end, CPUFREQ_RELATION_H, &freqEndIndex);

	/*
	if(direction == ADJ_STEP_UP) {
		// increase 33%
		freqadj = freqStartIndex - (freqStartIndex - freqEndIndex + 1)/3;
	}	
	else if(direction == ADJ_STEP_DOWN) {
		//decrease 33%
		freqadj = (freqStartIndex - freqEndIndex + 1)/3 + freqEndIndex;
	}
	else {
		// set cpu utilization ratio frequency from start to end index
		freqadj = (freqStartIndex - freqEndIndex + 1)*cpuUsage - 1;
		if(freqadj < 0)
			freqadj = 0;
		freqadj = (int)abs(freqadj/100 - freqStartIndex);
	}
	*/
	if((direction == ADJ_STEP_DOWN) || (direction == ADJ_SCALE_DOWN)) {
		//decrease 33%
		freqadj = (freqStartIndex - freqEndIndex + 1)/3 + freqEndIndex;
	}
	else {
		// set cpu utilization ratio frequency from start to end index
		freqadj = (freqStartIndex - freqEndIndex + 1)*cpuUsage - 1;
		if(freqadj < 0)
			freqadj = 0;
		freqadj = (int)abs(freqadj/100 - freqStartIndex);
	}
	
	if(((direction == ADJ_STEP_UP) || (direction == ADJ_SCALE_UP)) && (freqadj >= freqIndex) && (freqadj > freqEndIndex)) {
		freqadj = freqIndex - 1;
	}
	else if(((direction == ADJ_STEP_DOWN) || (direction == ADJ_SCALE_DOWN)) && (freqadj <= freqIndex) && (freqadj < freqStartIndex)) {
		freqadj = freqIndex + 1;
	}

	// prevent too frequent frequency scaling
	//if((direction == ADJ_STEP_UP) || (direction == ADJ_STEP_DOWN)) {
	if(direction == ADJ_STEP_DOWN) {
		//if((l_lazy_freq_step_up_cnt == vbfs_tuners_ins.lazy_freq_step_up_cnt) || (l_lazy_freq_step_down_cnt == vbfs_tuners_ins.lazy_freq_step_down_cnt)) {
		if(l_lazy_freq_step_down_cnt == vbfs_tuners_ins.lazy_freq_step_down_cnt) {
			vbfs_resources_ins.next_freq = vbfs_info->freq_table[freqadj].frequency;
			l_lazy_freq_step_up_cnt = l_lazy_freq_step_down_cnt = 0;
			if(vbfs_tuners_ins.debug == 1)
				pr_info("[DBG] stat : %d, lazy cnt over (%u/%u), use step frequency, cur : %u, next : %u\n", direction, vbfs_tuners_ins.lazy_freq_step_up_cnt, vbfs_tuners_ins.lazy_freq_step_down_cnt, policy->cur, vbfs_resources_ins.next_freq);
		}
		else {
			// maintain current frequency to prevent frequent step up or down
			vbfs_resources_ins.next_freq = policy->cur;
			if(vbfs_tuners_ins.debug == 1)
				pr_info("[DBG] stat : %d, lazy cnt under %u/%u(%u/%u), cur : %u\n", direction, vbfs_tuners_ins.lazy_freq_step_up_cnt, vbfs_tuners_ins.lazy_freq_step_down_cnt, l_lazy_freq_step_up_cnt, l_lazy_freq_step_down_cnt, policy->cur);
		}
	}
	else {
		vbfs_resources_ins.next_freq = vbfs_info->freq_table[freqadj].frequency;
	}

	if(vbfs_tuners_ins.debug == 1) {
		if((direction == ADJ_STEP_UP) || (direction == ADJ_STEP_DOWN)) {
			pr_info("[DBG] CPU Usage : %u(%d), need to step %s from %u to %u, status : %d\n", cpuUsage, vbfs_resources_ins.load_diff, direction == ADJ_STEP_UP?"up":"down", cpuFreq, vbfs_resources_ins.next_freq, direction);
		}
		else {
			pr_info("[DBG] CPU Usage : %u(%d), need to scale %s from %u to %u, status : %d\n", cpuUsage, vbfs_resources_ins.load_diff, direction == ADJ_SCALE_UP?"up":"down", cpuFreq, vbfs_resources_ins.next_freq, direction);
		}
	}

	return direction;
}

/*
 * check CPU unitilzation and its variation to find proper frequency to set
 */
int checkCPULoadCond(struct cpufreq_policy *policy, unsigned int cpuUsage, unsigned int cpuFreq, int cpuUsageDiff)
{
	unsigned int freq_start = 0, freq_end = 0;
	int freq_direction = ADJ_INVALID;

	if((cpuUsageDiff >= vbfs_tuners_ins.cpu_variation_up_threshold) || (cpuUsage >= vbfs_tuners_ins.cpu_usage_up_threshold))
	{
		if(cpuFreq == policy->max) {
			////pr_info("[DBG] CPU Usage : %d, need to step up but freq is already max %d\n", cpuUsage, cpuFreq);
			freq_direction = ADJ_SETTLED;
		}
		else {
			freq_start = cpuFreq;
			freq_end = policy->max;
			if(cpuUsageDiff >= vbfs_tuners_ins.cpu_variation_up_threshold) {
				freq_direction = ADJ_SCALE_UP;
			}
			else {
				freq_direction = ADJ_STEP_UP;
			}
		}
	}
	else if(((cpuUsageDiff <= vbfs_tuners_ins.cpu_variation_down_threshold) && (cpuUsage <= vbfs_tuners_ins.cpu_usage_down_threshold)) \
			|| (cpuUsage <= vbfs_tuners_ins.cpu_usage_down_threshold))		
	{
		if(cpuFreq == policy->min) {
			////pr_info("[DBG] CPU Usage : %d, need to step down but freq is already min %d\n", cpuUsage, cpuFreq);
			freq_direction = ADJ_SETTLED;
		}
		else {
			freq_start = policy->min;
			freq_end = cpuFreq;
			if((cpuUsageDiff <= vbfs_tuners_ins.cpu_variation_down_threshold) && (cpuUsage <= vbfs_tuners_ins.cpu_usage_down_threshold)) {
				freq_direction = ADJ_SCALE_DOWN;
			}
			else {
				freq_direction = ADJ_STEP_DOWN;
			}
		}
	}
	else {
		freq_direction = ADJ_NO_NEED;
	}

	switch(freq_direction)
	{
		case ADJ_SCALE_UP :
		case ADJ_SCALE_DOWN :
			l_lazy_freq_step_up_cnt = l_lazy_freq_step_down_cnt = 0;
			freq_direction = selectNextFrequencyByLoad(policy, cpuUsage, cpuFreq, freq_direction, freq_start, freq_end);
			break;
		case ADJ_STEP_UP : 
			l_lazy_freq_step_up_cnt++;
			l_lazy_freq_step_down_cnt = 0;
			freq_direction = selectNextFrequencyByLoad(policy, cpuUsage, cpuFreq, freq_direction, freq_start, freq_end);
			break;
		case ADJ_STEP_DOWN:
			l_lazy_freq_step_up_cnt = 0;
			l_lazy_freq_step_down_cnt ++;
			freq_direction = selectNextFrequencyByLoad(policy, cpuUsage, cpuFreq, freq_direction, freq_start, freq_end);
			break;
		case ADJ_SETTLED:
			l_lazy_freq_step_up_cnt = l_lazy_freq_step_down_cnt = 0;
			// set current frequency to turn or off cpus
			vbfs_resources_ins.next_freq = cpuFreq;
			break;
		case ADJ_NO_NEED :
		default :
			l_lazy_freq_step_up_cnt = l_lazy_freq_step_down_cnt = 0;
			// no need to change frequency
			vbfs_resources_ins.next_freq = 0;
			break;
	}

	return freq_direction;
}

/*
 * initialize vbfs resources and store variation, current resource values
 */
static int vbfs_initialize_resources(struct cpufreq_policy *policy)
{
	struct cpu_vbfs_info_s *vbfs_info;
	unsigned int index = 0;
	unsigned int thread_num = 0;
	int mem_portion = 0;
	int result = 0;

	vbfs_info = &per_cpu(od_cpu_vbfs_info, policy->cpu);
	
	vbfs_resources_ins.vbfs_adj = ADJ_NO_NEED;

	// if max and min frequency index is not initialize, set the up and return to use valid resource variations
	if((vbfs_resources_ins.maxfreqindex == 0) || (vbfs_resources_ins.minfreqindex == 0))
	{
		index = 0;
		cpufreq_frequency_table_target(policy, vbfs_info->freq_table, policy->max,
			CPUFREQ_RELATION_H, &index);
		vbfs_resources_ins.maxfreqindex = index;
		pr_info("[DBG] freq table max index : %u, freq : %u\n", vbfs_resources_ins.maxfreqindex, vbfs_info->freq_table[index].frequency);

		index = 0;
		cpufreq_frequency_table_target(policy, vbfs_info->freq_table, policy->min,
			CPUFREQ_RELATION_L, &index);
		vbfs_resources_ins.minfreqindex = index;
		pr_info("[DBG] freq table min index : %u, freq : %u\n", vbfs_resources_ins.minfreqindex, vbfs_info->freq_table[index].frequency);
		result = -1;
	}

	/* Get Absolute Load - in terms of freq */
	get_max_load_freq(policy);

	if(vbfs_resources_ins.max_load > 100) {
		pr_err("[DBG] load : %u(%d) overflow, set maximum load\n", vbfs_resources_ins.max_load, (int)vbfs_resources_ins.max_load);
		vbfs_resources_ins.max_load = 100;
	}
	
	vbfs_resources_ins.load_diff = (int)(vbfs_resources_ins.max_load - vbfs_resources_ins.load_prev);
	vbfs_resources_ins.load_prev = vbfs_resources_ins.max_load;

#ifdef CONFIG_CPU_THREAD_NUM
	thread_num = run_thread_number;
#else
	thread_num = 0;
#endif
	vbfs_resources_ins.thread_diff = thread_num - (int)vbfs_resources_ins.thread_prev;
	vbfs_resources_ins.thread_prev = (unsigned int)thread_num;

	mem_portion = get_mem_portion();
	vbfs_resources_ins.mem_diff = mem_portion - (int)vbfs_resources_ins.mem_prev;
	vbfs_resources_ins.mem_prev = (unsigned int)mem_portion;
	
	return result;
}

/*
 * check current variation of running threads #
 */
int checkThreadCond(int thread_diff)
{
	int threadcond = 0;
	
	if(thread_diff >= vbfs_tuners_ins.thread_variation_up_threshold) {
		threadcond = 1;
	}
	else if(thread_diff <= vbfs_tuners_ins.thread_variation_down_threshold) {
		threadcond = -1;
	}
	else {
		threadcond = 0;
	}

	return threadcond;
}

/*
 * check current variation of memory usage
 */
int checkMemCond(int mem_diff)
{
	int memcond = 0;
	
	if(mem_diff >= vbfs_tuners_ins.mem_variation_up_threshold) {
		memcond = 1;
	}
	else if(mem_diff <= vbfs_tuners_ins.mem_variation_down_threshold) {
		memcond = -1;
	}
	else {
		memcond = 0;
	}

	return memcond;
}

/*
 * adjust currently selected frequency if the variation of thread / memory is over or under the certain condition
 */
unsigned int adjustCond(struct cpufreq_policy *policy, unsigned int cpuFreq, int loadcond, int threadcond, int memcond)
{
	unsigned int adjfreq = 0, freqindex = 0;
	int need_to_ajust = 0, condpoint = 0;
	unsigned int direction = CPUFREQ_RELATION_L;

	struct cpu_vbfs_info_s *vbfs_info = &per_cpu(od_cpu_vbfs_info,
						   policy->cpu);

	condpoint = threadcond + memcond;

	switch(loadcond)
	{
		case ADJ_SETTLED:
			if(policy->cur == policy->max) {
				direction = CPUFREQ_RELATION_H;
			}
			else {
				direction = CPUFREQ_RELATION_L;
			}
			vbfs_resources_ins.vbfs_adj = ADJ_SETTLED;
			need_to_ajust = 0;
			break;
			
		case ADJ_STEP_UP:
		case ADJ_SCALE_UP:
			if(condpoint > 0) {
				adjfreq = vbfs_resources_ins.next_freq;
				direction = CPUFREQ_RELATION_H;
				vbfs_resources_ins.vbfs_adj = loadcond + ADJ_COND;
				need_to_ajust = 1;
			}
			else {
				direction = CPUFREQ_RELATION_H;
				vbfs_resources_ins.vbfs_adj = loadcond;
				need_to_ajust = 0;
			}
			break;

		case ADJ_STEP_DOWN:
		case ADJ_SCALE_DOWN:
			if(condpoint < 0) {
				adjfreq = vbfs_resources_ins.next_freq;
				direction = CPUFREQ_RELATION_L;
				vbfs_resources_ins.vbfs_adj = loadcond + ADJ_COND;
				need_to_ajust = 1;
			}
			else {
				direction = CPUFREQ_RELATION_L;
				vbfs_resources_ins.vbfs_adj = loadcond;
				need_to_ajust = 0;
			}
			break;
			
		case ADJ_NO_NEED:
			adjfreq = cpuFreq;
			need_to_ajust = 1;
			
			if(condpoint > 0) {
				direction = CPUFREQ_RELATION_H;
				vbfs_resources_ins.vbfs_adj = ADJ_COND_UP;
			}
			else if(condpoint < 0) {
				direction = CPUFREQ_RELATION_L;
				vbfs_resources_ins.vbfs_adj = ADJ_COND_DOWN;
			}
			else {
				direction = CPUFREQ_RELATION_L;
				vbfs_resources_ins.vbfs_adj = ADJ_NO_NEED;
				need_to_ajust = 0;
			}	
			break;

		default:
			direction = CPUFREQ_RELATION_L;
			need_to_ajust = 0;
			break;
	}

	if((need_to_ajust == 1) && (adjfreq != 0))
	{
		freqindex = 0;
		cpufreq_frequency_table_target(policy, vbfs_info->freq_table, (unsigned int)adjfreq,
						direction, &freqindex);

		switch(vbfs_resources_ins.vbfs_adj)
		{
			case ADJ_SCALE_N_COND_UP:
			case ADJ_STEP_N_COND_UP:
			case ADJ_COND_UP:
				while(condpoint > 0) {
					if(freqindex <= vbfs_resources_ins.maxfreqindex)
						break;
					freqindex--;
					condpoint--;
				}
				vbfs_resources_ins.next_freq = vbfs_info->freq_table[freqindex].frequency;
				if(vbfs_tuners_ins.debug == 1)
					pr_info("[DBG] frequency up %u -> %u -> %u\n", cpuFreq, adjfreq, vbfs_resources_ins.next_freq);
				break;
				
			case ADJ_SCALE_N_COND_DOWN:
			case ADJ_STEP_N_COND_DOWN:
			case ADJ_COND_DOWN :
				while(condpoint < 0) {
					if(freqindex >= vbfs_resources_ins.minfreqindex)
						break;
					freqindex++;
					condpoint++;
				}
				vbfs_resources_ins.next_freq = (int)vbfs_info->freq_table[freqindex].frequency;
				if(vbfs_tuners_ins.debug == 1)
					pr_info("[DBG] frequency down %u -> %u -> %u\n", cpuFreq, adjfreq, vbfs_resources_ins.next_freq);
				break;
				
			default:
				break;
		}
	}

	return direction;
}

void get_cpu_onlines(struct cpufreq_policy *policy, char *buf, int size)
{
	unsigned int j = 0, mask = 0;
	int count = 0;
	
	for_each_cpu(j, policy->cpus) {
		if (cpu_online(j)) {
			mask |= 1 << j;
		}
	}

	if(size >= nr_cpu_ids) {
		count = nr_cpu_ids;
	}
	else {
		count = size;
	}

	for(j = 0; j < count; j++)
	{
		buf[j] = '0' + (char)((mask >> j)&0x00000001);
	}
	
}

/*
 * write logs on /data/cpulogs/##
 */
void vbfs_write_logs(struct cpufreq_policy *policy)
{
	char file_buf[100];
	unsigned long long t;
	unsigned long nanosec_rem;
	int this_cpu;

	char cpu_online_mask[10];

	if(get_status_monitor_flag() == 1)
	{
		memset(file_buf, 0, sizeof(file_buf));
		memset(cpu_online_mask, 0, sizeof(cpu_online_mask));
	
		this_cpu = smp_processor_id();
		t = cpu_clock(this_cpu);
		nanosec_rem = do_div(t, 1000000000);

		if(vbfs_resources_ins.t_init == 0)
		{
			vbfs_resources_ins.t_init = vbfs_resources_ins.t_prev = t;
			printk(KERN_INFO " %d", 0);
		}
		else if(vbfs_resources_ins.t_prev != t)
		{
			vbfs_resources_ins.t_prev = t;
			printk(KERN_INFO " %d", (int)(vbfs_resources_ins.t_prev-vbfs_resources_ins.t_init));
		}

		get_cpu_onlines(policy, cpu_online_mask, sizeof(cpu_online_mask));
		//mem_portion = get_mem_portion();
		sprintf(file_buf, "%5lu.%06lu, %7u, %9d, %9u, %7d, %7d, %7s, %7d\n", (unsigned long) t, nanosec_rem / 1000, vbfs_resources_ins.load_prev, vbfs_resources_ins.max_load_freq, policy->cur, vbfs_resources_ins.thread_prev, vbfs_resources_ins.mem_prev, cpu_online_mask, vbfs_resources_ins.vbfs_adj);
		status_monitor_write_file(file_buf, (unsigned int)strlen(file_buf), 0);
	}
	else if(vbfs_resources_ins.t_init != 0)
	{
		vbfs_resources_ins.t_init = 0;
	}
	
	
}

static void vbfs_check_resources(struct cpu_vbfs_info_s *this_vbfs_info)
{
	struct cpufreq_policy *policy;
	int status = -1;
	unsigned int freq_direction = CPUFREQ_RELATION_L;
	int loadcond = 0, threadcond = 0, memcond = 0;

	this_vbfs_info->freq_lo = 0;
	policy = this_vbfs_info->cur_policy;

	status = vbfs_initialize_resources(policy);
	if(status < 0) {
		pr_info("%s initialized, return\n", __func__);
		return;
	}

	loadcond = checkCPULoadCond(policy, vbfs_resources_ins.max_load, policy->cur, vbfs_resources_ins.load_diff);
	if(loadcond == ADJ_INVALID) {
		pr_err("[DBG] invalid frequency decision, return\n");
		return;
	}

	threadcond = checkThreadCond(vbfs_resources_ins.thread_diff);
	memcond = checkMemCond(vbfs_resources_ins.mem_diff);

	freq_direction = adjustCond(policy, policy->cur, loadcond, threadcond, memcond);
	
#ifdef CONFIG_CPU_FREQ_DBG
	vbfs_write_logs(policy);
#endif

	// if current frequency is required to be changed, set the selected frequency
	if((vbfs_resources_ins.vbfs_adj != ADJ_NO_NEED) && (vbfs_resources_ins.vbfs_adj != ADJ_INVALID) && (l_vbfs_cpufreq_set_flag == 0))
	{
		if((vbfs_tuners_ins.debug == 1) && (vbfs_resources_ins.vbfs_adj != ADJ_SETTLED))
			pr_info("[DBG ADDJUST] Cur : %d, Next : %d, CPU usage : %d(%d), T : %d, M : %d, A: %d\n", (int)policy->cur, vbfs_resources_ins.next_freq, (int)vbfs_resources_ins.load_prev, vbfs_resources_ins.load_diff, threadcond, memcond, vbfs_resources_ins.vbfs_adj);
		if (!vbfs_tuners_ins.powersave_bias) {
			__cpufreq_driver_target(policy, vbfs_resources_ins.next_freq,
					freq_direction);
		} else {
			int freq = powersave_bias_target(policy, vbfs_resources_ins.next_freq,
					CPUFREQ_RELATION_L);
			__cpufreq_driver_target(policy, freq,
				CPUFREQ_RELATION_L);
		}
	}

}

static void do_vbfs_timer(struct work_struct *work)
{
	struct cpu_vbfs_info_s *vbfs_info =
		container_of(work, struct cpu_vbfs_info_s, work.work);
	unsigned int cpu = vbfs_info->cpu;
	int sample_type = vbfs_info->sample_type;

	int delay;

	mutex_lock(&vbfs_info->timer_mutex);

	/* Common NORMAL_SAMPLE setup */
	vbfs_info->sample_type = VBFS_NORMAL_SAMPLE;
	if (!vbfs_tuners_ins.powersave_bias ||
	    sample_type == VBFS_NORMAL_SAMPLE) {
		vbfs_check_resources(vbfs_info);
		if (vbfs_info->freq_lo) {
			/* Setup timer for SUB_SAMPLE */
			vbfs_info->sample_type = VBFS_SUB_SAMPLE;
			delay = vbfs_info->freq_hi_jiffies;
		} else {
			/* We want all CPUs to do sampling nearly on
			 * same jiffy
			 */
			delay = usecs_to_jiffies(vbfs_tuners_ins.sampling_rate
				* vbfs_info->rate_mult);

			if (num_online_cpus() > 1)
				delay -= jiffies % delay;
		}
	} else {
		__cpufreq_driver_target(vbfs_info->cur_policy,
			vbfs_info->freq_lo, CPUFREQ_RELATION_H);
		delay = vbfs_info->freq_lo_jiffies;
	}
	schedule_delayed_work_on(cpu, &vbfs_info->work, delay);
	mutex_unlock(&vbfs_info->timer_mutex);
}

static inline void vbfs_timer_init(struct cpu_vbfs_info_s *vbfs_info)
{
	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay = usecs_to_jiffies(vbfs_tuners_ins.sampling_rate);

#ifdef CONFIG_CPU_FREQ_DBG
	pr_info("[DBG] %s\n", __func__);
#endif

	if (num_online_cpus() > 1)
		delay -= jiffies % delay;

	vbfs_info->sample_type = VBFS_NORMAL_SAMPLE;
	INIT_DELAYED_WORK_DEFERRABLE(&vbfs_info->work, do_vbfs_timer);
	schedule_delayed_work_on(vbfs_info->cpu, &vbfs_info->work, delay);
}

static inline void vbfs_timer_exit(struct cpu_vbfs_info_s *vbfs_info)
{
	cancel_delayed_work_sync(&vbfs_info->work);
}

/*
 * Not all CPUs want IO time to be accounted as busy; this dependson how
 * efficient idling at a higher frequency/voltage is.
 * Pavel Machek says this is not so for various generations of AMD and old
 * Intel systems.
 * Mike Chan (androidlcom) calis this is also not true for ARM.
 * Because of this, whitelist specific known (series) of CPUs by default, and
 * leave all others up to the user.
 */
static int should_io_be_busy(void)
{
#if defined(CONFIG_X86)
	/*
	 * For Intel, Core 2 (model 15) andl later have an efficient idle.
	 */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL &&
	    boot_cpu_data.x86 == 6 &&
	    boot_cpu_data.x86_model >= 15)
		return 1;
#endif
	return 0;
}

static int cpufreq_governor_vbfs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	unsigned int cpu = policy->cpu;
	struct cpu_vbfs_info_s *this_vbfs_info;
	unsigned int j;
	int rc;

	this_vbfs_info = &per_cpu(od_cpu_vbfs_info, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
#ifdef CONFIG_CPU_FREQ_DBG
		pr_info("[DBG] %s START, cpu : %d\n", __func__, cpu);
#endif
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&vbfs_mutex);

		vbfs_enable++;
		for_each_cpu(j, policy->cpus) {
			struct cpu_vbfs_info_s *j_vbfs_info;
			j_vbfs_info = &per_cpu(od_cpu_vbfs_info, j);
			j_vbfs_info->cur_policy = policy;

			j_vbfs_info->prev_cpu_idle = get_cpu_idle_time(j,
						&j_vbfs_info->prev_cpu_wall);
			if (vbfs_tuners_ins.ignore_nice) {
				j_vbfs_info->prev_cpu_nice =
						kstat_cpu(j).cpustat.nice;
			}
		}
		this_vbfs_info->cpu = cpu;
		this_vbfs_info->rate_mult = 1;
		vbfs_powersave_bias_init_cpu(cpu);
		/*
		 * Start the timerschedule work, when this governor
		 * is used for first time
		 */
		if (vbfs_enable == 1) {
			unsigned int latency;
#ifdef CONFIG_CPU_FREQ_DBG
			pr_info("[DBG] %s START, cpu : %d, create attr group\n", __func__, cpu);
#endif
			rc = sysfs_create_group(cpufreq_global_kobject,
						&vbfs_attr_group);
			if (rc) {
				mutex_unlock(&vbfs_mutex);
				return rc;
			}

			/* policy latency is in nS. Convert it to uS first */
			latency = policy->cpuinfo.transition_latency / 1000;
			if (latency == 0)
				latency = 1;
			/* Bring kernel and HW constraints together */
			min_sampling_rate = max(min_sampling_rate,
					MIN_LATENCY_MULTIPLIER * latency);
			vbfs_tuners_ins.sampling_rate =
				max(min_sampling_rate,
				    latency * LATENCY_MULTIPLIER);
			vbfs_tuners_ins.io_is_busy = should_io_be_busy();
		}
		mutex_unlock(&vbfs_mutex);

		mutex_init(&this_vbfs_info->timer_mutex);
		vbfs_timer_init(this_vbfs_info);
		break;

	case CPUFREQ_GOV_STOP:
#ifdef CONFIG_CPU_FREQ_DBG
		pr_info("[DBG] %s STOP, cpu : %d\n", __func__, cpu);
#endif
		vbfs_timer_exit(this_vbfs_info);

		mutex_lock(&vbfs_mutex);
		mutex_destroy(&this_vbfs_info->timer_mutex);
		vbfs_enable--;
		mutex_unlock(&vbfs_mutex);
		if (!vbfs_enable)
			sysfs_remove_group(cpufreq_global_kobject,
					   &vbfs_attr_group);

		break;

	case CPUFREQ_GOV_LIMITS:
#ifdef CONFIG_CPU_FREQ_DBG
		pr_info("[DBG] %s LIMITS, cpu : %d\n", __func__, cpu);
#endif
		mutex_lock(&this_vbfs_info->timer_mutex);
		if (policy->max < this_vbfs_info->cur_policy->cur)
			__cpufreq_driver_target(this_vbfs_info->cur_policy,
				policy->max, CPUFREQ_RELATION_H);
		else if (policy->min > this_vbfs_info->cur_policy->cur)
			__cpufreq_driver_target(this_vbfs_info->cur_policy,
				policy->min, CPUFREQ_RELATION_L);
		mutex_unlock(&this_vbfs_info->timer_mutex);
		break;
	}
	return 0;
}

static int __init cpufreq_gov_vbfs_init(void)
{
	cputime64_t wall;
	u64 idle_time;
	int cpu = get_cpu();

#ifdef CONFIG_CPU_FREQ_DBG
	pr_info("[DBG] %s\n", __func__);
#endif

	idle_time = get_cpu_idle_time_us(cpu, &wall);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		vbfs_tuners_ins.cpu_usage_up_threshold = CPUUSAGE_MIN_UP_THRESHOLD;
		vbfs_tuners_ins.cpu_usage_down_threshold = CPUUSAGE_MIN_DOWN_THRESHOLD;
		vbfs_tuners_ins.cpu_variation_up_threshold = CPUUSAGE_UP_THRESHOLD;
		vbfs_tuners_ins.cpu_variation_down_threshold = CPUUSAGE_DOWN_THRESHOLD;
		vbfs_tuners_ins.thread_variation_up_threshold = THREAD_UP_THRESHOLD;
		vbfs_tuners_ins.thread_variation_down_threshold = THREAD_DOWN_THRESHOLD;
		vbfs_tuners_ins.mem_variation_up_threshold = MEM_UP_THRESHOLD;
		vbfs_tuners_ins.mem_variation_down_threshold = MEM_DOWN_THRESHOLD;

		/*
		 * In no_hz/micro accounting case we set the minimum frequency
		 * not depending on HZ, but fixed (very low). The deferred
		 * timer might skip some samples if idle/sleeping as needed.
		*/
		min_sampling_rate = MICRO_FREQUENCY_MIN_SAMPLE_RATE;
	} else {
		/* For correct statistics, we need 10 ticks for each measure */
		min_sampling_rate =
			MIN_SAMPLING_RATE_RATIO * jiffies_to_usecs(10);
	}

	return cpufreq_register_governor(&cpufreq_gov_vbfs);
}

static void __exit cpufreq_gov_vbfs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_vbfs);
}


MODULE_AUTHOR("Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>");
MODULE_AUTHOR("Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>");
MODULE_DESCRIPTION("'cpufreq_vbfs' - A dynamic cpufreq governor "
	"based on resource variations on reference to the ondemand governor");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_VBFS
fs_initcall(cpufreq_gov_vbfs_init);
#else
module_init(cpufreq_gov_vbfs_init);
#endif
module_exit(cpufreq_gov_vbfs_exit);

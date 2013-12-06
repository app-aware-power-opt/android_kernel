
#define GOVERNOR_LENGTH 20
#define CPUFREQ_MIN 200000
#define CPUFREQ_MAX 1704000

typedef enum{
    G_ADAPTIVE, G_INTERACTIVE, G_CONSERVATIVE, G_USERSPACE, G_POWERSAVE, G_ONDEMAND, G_PERFORMANCE, G_MAX_GOV
}cpufreq_governor_t;

unsigned int read_scaling_governor(void);
void set_scaling_governor(unsigned int scaling_governor);
void set_cpufreq_to_max(void);
void set_cpufreq_to_min(void);
void set_cpufreq_to_value(unsigned int freq);
unsigned int read_scaling_cur_freq(void);
unsigned int get_next_freq(unsigned int cur_freq);
void set_cpufreq_to_next_step(void);
unsigned int get_prev_freq(unsigned int cur_freq);
void set_cpufreq_to_prev_step(void);
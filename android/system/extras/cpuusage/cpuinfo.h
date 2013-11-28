


typedef struct {
    unsigned int scaling_cur_freq;
    int run_thread;
} cpuinfo_t;

void read_cpu0_cur_freq(cpuinfo_t *c);
void read_cpu1_cur_freq(cpuinfo_t *c);
void read_cpu2_cur_freq(cpuinfo_t *c);
void read_cpu3_cur_freq(cpuinfo_t *c);

void read_run_thread_number(cpuinfo_t *c);



/*
 * cpuusage.c  
 * softgear@etri.re.kr 2011.4.22
 *
 * Tirqs code reads /proc/stat twice and calculate cpu usage as %.
 * We do not use float.
 */ 
    
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <linux/time.h>
#include <sys/time.h>
#include <sys/stat.h>
//#include <sys/type.h>
#include <fcntl.h>
#include <errno.h>
//#include <linux/jiffies.h>
//#include <error.h>
//#include <errno.h>

#ifdef PROC_MEM
#include <dirent.h>
#include <sys/types.h>
#include <pagemap/pagemap.h>
#endif

#include "scoreCalc.h"

#define INTERVAL 2      /* 5 secs */
#define SCORECALC
#define FILE_PATH (mnt/ext_usb    
typedef struct {
    char cpu[20];
    unsigned long long user;
    unsigned long long nice;
	
    unsigned long long system;

    unsigned long long idle;
    unsigned long long iowait;

    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
    unsigned long long guest;
    unsigned long long guest_nice;
	
} procstat_t;

#ifdef CPU_FREQ
typedef struct {
    unsigned int scaling_cur_freq;
    int run_thread;
} cpuinfo_t;
#endif
 
void read_proc_stat(procstat_t *p)
{
    FILE *fp;
    int ret;
 
    fp = fopen("/proc/stat", "r");
    if(fp == NULL)
    {
       // fprintf(stderr, "read /proc/stat error : %s\n", strerror(errno));
        exit(-1);
    }
 
    /* 
	cpu   user  nice  system    idle  iowait   irq  softirq  steal  guest	guest_nice
	cpu   2255    34    2290  22625   6290    127     456      0       0
	cpu0  1132    34    1441  11311   3675    127     438      0       0
	cpu1  1123     0    849   11313   2614      0      18      0       0
     */
    ret = fscanf(fp, "%s %llu %llu %llu %llu %llu %llu %llu %llu  %llu %llu",
        p->cpu,
        &p->user,
        &p->nice,        
        &p->system,
		&p->idle,
        &p->iowait,
        &p->irq,
        &p->softirq,
        &p->steal,
        &p->guest,
        &p->guest_nice
		);
 
    fclose(fp);
}

#ifdef CPU_FREQ
void read_cpu0_cur_freq(cpuinfo_t *c)
{
    FILE *fp;
    int ret;
 
    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if(fp == NULL)
    {
       // fprintf(stderr, "read /proc/stat error : %s\n", strerror(errno));
        exit(-1);
    }
 

    ret = fscanf(fp, "%d", &c->scaling_cur_freq );
 
    fclose(fp);
}

void read_cpu1_cur_freq(cpuinfo_t *c)
{
    FILE *fp;
    int ret;
 
    fp = fopen("/sys/devices/system/cpu/cpu1/online", "r");
    if(fp == NULL)
    {
       // fprintf(stderr, "read /proc/stat error : %s\n", strerror(errno));
        exit(-1);
    }
 

    ret = fscanf(fp, "%d", &c->scaling_cur_freq );
 
    fclose(fp);
}

void read_cpu2_cur_freq(cpuinfo_t *c)
{
    FILE *fp;
    int ret;
 
    fp = fopen("/sys/devices/system/cpu/cpu2/online", "r");
    if(fp == NULL)
    {
       // fprintf(stderr, "read /proc/stat error : %s\n", strerror(errno));
        exit(-1);
    }
 

    ret = fscanf(fp, "%d", &c->scaling_cur_freq );
 
    fclose(fp);
}

void read_cpu3_cur_freq(cpuinfo_t *c)
{
    FILE *fp;
    int ret;
 
    fp = fopen("/sys/devices/system/cpu/cpu3/online", "r");
    if(fp == NULL)
    {
       // fprintf(stderr, "read /proc/stat error : %s\n", strerror(errno));
        exit(-1);
    }
 

    ret = fscanf(fp, "%d", &c->scaling_cur_freq );
 
    fclose(fp);
}
#endif

#ifdef CPU_THREAD
void read_run_thread_number(cpuinfo_t *c)
{
    FILE *fp;
    int ret;
 
    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/run_thread", "r");
    if(fp == NULL)
    {
       // fprintf(stderr, "read /proc/stat error : %s\n", strerror(errno));
        exit(-1);
    }
 

    ret = fscanf(fp, "%d", &c->run_thread);
 
    fclose(fp);
}
#endif

/*
 * import this source from system/extras/procrankprocrank.c
 * PSS will be used
 */
#ifdef PROC_MEM
struct proc_info {
    pid_t pid;
    pm_memusage_t usage;
    unsigned long wss;
};

static int getprocname(pid_t pid, char *buf, int len);
static int numcmp(long long a, long long b);

#define declare_sort(field) \
    static int sort_by_ ## field (const void *a, const void *b)

declare_sort(vss);
declare_sort(rss);
declare_sort(pss);
declare_sort(uss);

int (*compfn)(const void *a, const void *b);
static int order;



/*
 * Get the process name for a given PID. Inserts the process name into buffer
 * buf of length len. The size of the buffer must be greater than zero to get
 * any useful output.
 *
 * Note that fgets(3) only declares length as an int, so our buffer size is
 * also declared as an int.
 *
 * Returns 0 on success, a positive value on partial success, and -1 on
 * failure. Other interesting values:
 *   1 on failure to create string to examine proc cmdline entry
 *   2 on failure to open proc cmdline entry
 *   3 on failure to read proc cmdline entry
 */
static int getprocname(pid_t pid, char *buf, int len) {
    char *filename;
    FILE *f;
    int rc = 0;
    static const char* unknown_cmdline = "<unknown>";

    if (len <= 0) {
        return -1;
    }

    if (asprintf(&filename, "/proc/%zd/cmdline", pid) < 0) {
        rc = 1;
        goto exit;
    }

    f = fopen(filename, "r");
    if (f == NULL) {
        rc = 2;
        goto releasefilename;
    }

    if (fgets(buf, len, f) == NULL) {
        rc = 3;
        goto closefile;
    }

closefile:
    (void) fclose(f);
releasefilename:
    free(filename);
exit:
    if (rc != 0) {
        /*
         * The process went away before we could read its process name. Try
         * to give the user "<unknown>" here, but otherwise they get to look
         * at a blank.
         */
        if (strlcpy(buf, unknown_cmdline, (size_t)len) >= (size_t)len) {
            rc = 4;
        }
    }

    return rc;
}

static int numcmp(long long a, long long b) {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

#define create_sort(field, compfn) \
    static int sort_by_ ## field (const void *a, const void *b) { \
        return order * compfn( \
            (*((struct proc_info**)a))->usage.field, \
            (*((struct proc_info**)b))->usage.field \
        ); \
    }

create_sort(vss, numcmp)
create_sort(rss, numcmp)
create_sort(pss, numcmp)
create_sort(uss, numcmp)

/*
 * return total available memory
 */
static long mem[] = { 0, 0, 0, 0, 0, 0 };
long print_mem_info(void) {
    char buffer[1024];
    int numFound = 0;

    int fd = open("/proc/meminfo", O_RDONLY);

    if (fd < 0) {
        printf("Unable to open /proc/meminfo: %s\n", strerror(errno));
        return NULL;
    }

    const int len = read(fd, buffer, sizeof(buffer)-1);
    close(fd);

    if (len < 0) {
        printf("Empty /proc/meminfo");
        return NULL;
    }
    buffer[len] = 0;

    static const char* const tags[] = {
            "MemTotal:",
            "MemFree:",
            "Buffers:",
            "Cached:",
            "Shmem:",
            "Slab:",
            NULL
    };
    static const int tagsLen[] = {
            9,
            8,
            8,
            7,
            6,
            5,
            0
    };
   // long mem[] = { 0, 0, 0, 0, 0, 0 };
   memset(mem, 0, sizeof(mem));

    char* p = buffer;
    while (*p && numFound < 6) {
        int i = 0;
        while (tags[i]) {
            if (strncmp(p, tags[i], tagsLen[i]) == 0) {
                p += tagsLen[i];
                while (*p == ' ') p++;
                char* num = p;
                while (*p >= '0' && *p <= '9') p++;
                if (*p != 0) {
                    *p = 0;
                    p++;
                }
                mem[i] = atoll(num);
                numFound++;
                break;
            }
            i++;
        }
        while (*p && *p != '\n') {
            p++;
        }
        if (*p) p++;
    }

    //printf("	RAM: %ldK total, %ldK free, %ldK buffers, %ldK cached, %ldK shmem, %ldK slab\n\n",
    //        mem[0], mem[1], mem[2], mem[3], mem[4], mem[5]);

    return mem[0];
}

/*
 * return the portion of PSS out of Total memory
 */
float get_mem_info(void)
{
    pm_kernel_t *ker;
    pm_process_t *proc;
    pid_t *pids;
    struct proc_info **procs;
    size_t num_procs;
    unsigned long total_vss = 0;
    unsigned long total_rss = 0;
    unsigned long total_pss = 0;
    unsigned long total_uss = 0;
    unsigned long total_mem = 0;
    int ws;
    char cmdline[256]; // this must be within the range of int
    int error;
    size_t i, j;

    #define WS_OFF   0
    #define WS_ONLY  1
    #define WS_RESET 2

    compfn = &sort_by_pss;
    order = -1;
    ws = WS_OFF;

    error = pm_kernel_create(&ker);
    if (error) {
        fprintf(stderr, "Error creating kernel interface -- "
                        "does this kernel have pagemap?\n");
        return -1;
    }

    error = pm_kernel_pids(ker, &pids, &num_procs);
    if (error) {
        fprintf(stderr, "Error listing processes.\n");
        return -1;
    }

    procs = calloc(num_procs, sizeof(struct proc_info*));
    if (procs == NULL) {
        fprintf(stderr, "calloc: %s", strerror(errno));
        return -1;
    }

    for (i = 0; i < num_procs; i++) {
        procs[i] = malloc(sizeof(struct proc_info));
        if (procs[i] == NULL) {
            fprintf(stderr, "malloc: %s\n", strerror(errno));
            return -1;
        }
        procs[i]->pid = pids[i];
        pm_memusage_zero(&procs[i]->usage);
        error = pm_process_create(ker, pids[i], &proc);
        if (error) {
            fprintf(stderr, "warning: could not create process interface for %d\n", pids[i]);
            continue;
        }

        error = pm_process_usage(proc, &procs[i]->usage);
        if (error) {
            fprintf(stderr, "warning: could not read usage for %d\n", pids[i]);
        }

        pm_process_destroy(proc);
    }

    free(pids);

    j = 0;
    for (i = 0; i < num_procs; i++) {
        if (procs[i]->usage.vss) {
            procs[j++] = procs[i];
        } else {
            free(procs[i]);
        }
    }
    num_procs = j;

    qsort(procs, num_procs, sizeof(procs[0]), compfn);

    printf("Show up to 5 processes in order of PSS\n");
    printf("%5s  %8s  %8s  %8s  %8s  %s\n", "PID", "Vss", "Rss", "Pss", "Uss", "cmdline");

    total_vss = 0;
    total_rss = 0;
    total_pss = 0;
    total_uss = 0;

    for (i = 0; i < num_procs; i++) {
        if (getprocname(procs[i]->pid, cmdline, (int)sizeof(cmdline)) < 0) {
            /*
             * Something is probably seriously wrong if writing to the stack
             * failed.
             */
            free(procs[i]);
            continue;
        }

        total_vss += procs[i]->usage.vss;
        total_rss += procs[i]->usage.rss;
        total_pss += procs[i]->usage.pss;
        total_uss += procs[i]->usage.uss;

        if(i < 5)
        {
            printf("%5d  %7dK  %7dK  %7dK  %7dK  %s\n",
                    procs[i]->pid,
                    procs[i]->usage.vss / 1024,
                    procs[i]->usage.rss / 1024,
                    procs[i]->usage.pss / 1024,
                    procs[i]->usage.uss / 1024,
                    cmdline
            );
        }

        free(procs[i]);
    }

    free(procs);

    total_vss /= 1024;
    total_rss /= 1024;
    total_pss /= 1024;
    total_uss /= 1024;
    
    printf("%5s  %8s  %8s  %8s  %8s  %s\n",
            "", "------", "------", "------", "------", "------");
    printf("%5s  %7ldK  %7ldK  %7ldK  %7ldK  %s\n",
            "", total_vss, total_rss, total_pss, total_uss, "TOTAL");

    total_mem = print_mem_info();
    if(total_mem <= 0)
    {
        fprintf(stderr, "Error getting available memory\n");
        return -1;
    }
    printf("%5s  %7.2f%%  %7.2f%%  %7.2f%%  %7.2f%%  %s(%7ldK)\n",
            "", (float)total_vss*100/total_mem, (float)total_rss*100/total_mem, (float)total_pss*100/total_mem, (float)total_uss*100/total_mem, "RAM TOTAL", total_mem);
    printf("%5s  RAM: %ldK total, %ldK free, %ldK buffers, %ldK cached\n\n",
            "", mem[0], mem[1], mem[2], mem[3]);

    return (float)total_pss*100/total_mem;
}
#endif
 
/* d = a - b */
void diff_proc_stat(procstat_t *a, procstat_t *b, procstat_t *d)
{
    d->user     = a->user   - b->user;
    d->nice     = a->nice   - b->nice;	
    d->system   = a->system	- b->system;
    d->idle     = a->idle   - b->idle;
    d->iowait   = a->iowait	- b->iowait;
    d->irq      = a->irq    - b->irq;
    d->softirq  = a->softirq  - b->softirq;
    d->steal     = a->steal   - b->steal;
    d->guest     = a->guest   - b->guest;	
	d->guest_nice     = a->guest_nice   - b->guest_nice;	
}

int main(int argc, char* argv[])
{
    procstat_t m1,m2;
    procstat_t d;
    procstat_t u100; /* percentages(%x100) per total */

#ifdef CPU_FREQ
    cpuinfo_t c0;
    cpuinfo_t c1;
    cpuinfo_t c2;
    cpuinfo_t c3;
#endif

    unsigned long long total, usage100;
    FILE *ofp = NULL;
    time_t old, new;
    clock_t timeStamp = 0;
	
    int i = 0;
    int isFirst = 1;

    int logInterval = 0;
    int logTime = 0;

#ifdef PROC_MEM
    float pss_portion = 0;
#endif

    if (argc != 3) {
        printf("Usage : ./cpu-daemon [interval (ms)] [logging time (s)]\n");
        return 0;
    }

    for(i = 1; i<argc; i++) {
        printf("%s ", argv[i]);
    }

    logInterval = atoi(argv[1]) * 1000;
    logTime = atoi(argv[2]);

    printf("\nlog Interval = %d[us] log Time = %d[s]\n", logInterval, logTime);
		
    time(&old);
	
    while(1)
    {
        time(&new);
        if(difftime(new, old) < logTime)
        {
            timeStamp = clock();
            read_proc_stat(&m1);
            usleep(logInterval);
            read_proc_stat(&m2);
            diff_proc_stat(&m2, &m1, &d);

#ifdef CPU_THREAD
            read_run_thread_number(&c0);
#endif

#ifdef CPU_FREQ
            read_cpu0_cur_freq(&c0);
            read_cpu1_cur_freq(&c1);
            read_cpu2_cur_freq(&c2);
            read_cpu3_cur_freq(&c3);
#endif

#ifdef PROC_MEM
            pss_portion = get_mem_info();
            //printf("PSS : %7.2f%%\n", pss_portion);
#endif

            total = d.user + d.system + d.nice + d.idle + d.iowait + d.irq + d.softirq + d.steal + d.guest + d.guest_nice;
            u100.user =		d.user * 10000 / total;
            u100.nice =		d.nice * 10000 / total;			
            u100.system =	d.system * 10000 / total;
			
            u100.idle =		d.idle * 10000 / total;
            u100.iowait =	d.iowait * 10000 / total;
            u100.irq =		d.irq * 10000 / total;
            u100.softirq =	d.softirq * 10000 / total;

            u100.steal =	d.steal * 10000 / total;
            u100.guest =	d.guest * 10000 / total;
            u100.guest_nice =	d.guest_nice * 10000 / total;

            usage100 =		10000 - u100.idle;

            if(isFirst)
            {
                isFirst = 0;

                if (mkdir("/mnt/ext_usb/cpulog", 775) == -1 && errno != EEXIST) {
                    fprintf(stderr, "Problem creating directory");
                    perror(" ");
                }
				
                ofp = fopen("/mnt/ext_usb/cpulog/cpulog.txt", "w");
                if(ofp == NULL)
                {
                    printf("fopen w error\n");
                    return 0;
                }

#if defined(CPU_THREAD) && defined(CPU_FREQ) && defined(PROC_MEM)
                fprintf(ofp, "[%5.5s],[%5.5s],[%5.5s], [%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s]\n", "time","cpu","thread", "freq0", "freq1","freq2","freq3", "mem");
#elif defined (CPU_THREAD) && defined(CPU_FREQ) 
                fprintf(ofp, "[%5.5s],[%5.5s],[%5.5s], [%5.5s],[%5.5s],[%5.5s],[%5.5s]\n", "time","cpu","thread", "freq0", "freq1","freq2","freq3");
#else
                fprintf(ofp, "[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s]\n", "time","cpu","user","nice","system","idle","iowait","irq","softirq","steal","guest","guest_nice");
#endif
				
                fclose(ofp);
#if defined(CPU_THREAD) && defined(CPU_FREQ) && defined(PROC_MEM)
                printf("[%5.5s],[%5.5s],[%5.5s],[%5.5s], [%5.5s],[%5.5s],[%5.5s],[%5.5s]\n", "time","cpu","thread","freq0", "freq1","freq2","freq3", "mem");
#elif defined (CPU_THREAD) && defined(CPU_FREQ) 
                printf("[%5.5s],[%5.5s],[%5.5s],[%5.5s], [%5.5s],[%5.5s],[%5.5s]\n", "time","cpu","thread","freq0", "freq1","freq2","freq3");
#else
                printf( "[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s]\n", "time","cpu","user","nice","system","idle","iowait","irq","softirq","steal","guest","guest_nice");
#endif
            }
			
#if defined(CPU_THREAD) && defined(CPU_FREQ)
            printf("%6.2fs,%4llu.%02llu, %7d,%7d, %7d, %7d, %7d, %4.2f \n",
                    (double)timeStamp/CLOCKS_PER_SEC,
                    usage100/100, usage100%100,
                    c0.run_thread,
                    c0.scaling_cur_freq,
                    c1.scaling_cur_freq,
                    c2.scaling_cur_freq,
                    c3.scaling_cur_freq,
                    pss_portion
            );
#elif defined (CPU_THREAD) && defined(CPU_FREQ) 
            printf("%6.2fs,%4llu.%02llu, %7d,%7d, %7d, %7d, %7d \n",
                    (double)timeStamp/CLOCKS_PER_SEC,
                    usage100/100, usage100%100,
                    c0.run_thread,
                    c0.scaling_cur_freq,
                    c1.scaling_cur_freq,
                    c2.scaling_cur_freq,
                    c3.scaling_cur_freq
            );
#else
            printf("%6.2fs,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu\n",
                    (double)timeStamp/CLOCKS_PER_SEC,
                    usage100/100, usage100%100,       
                    u100.user/100, u100.user%100,
                    u100.nice/100, u100.nice%100,				
                    u100.system/100, u100.system%100,

                    u100.idle/100, u100.idle%100,
                    u100.iowait/100, u100.iowait%100,
                    u100.irq/100, u100.irq%100,
                    u100.softirq/100, u100.softirq%100,

                    u100.steal/100, u100.steal%100,
                    u100.guest/100, u100.guest%100,
                    u100.guest_nice/100, u100.guest_nice%100
                );
#endif

            ofp = fopen("/mnt/ext_usb/cpulog/cpulog.txt", "a");
            if(ofp == NULL)
            {
                printf("fopen a error\n");
                return 0;
            }		

#if defined(CPU_THREAD) && defined(CPU_FREQ)
            fprintf(ofp, "%6.2fs,%4llu.%02llu, %7d, %7d, %7d, %7d, %7d, %4.2f \n",
                    (double)timeStamp/CLOCKS_PER_SEC,
                    usage100/100, usage100%100,
                    c0.run_thread,
                    c0.scaling_cur_freq,
                    c1.scaling_cur_freq,
                    c2.scaling_cur_freq,
                    c3.scaling_cur_freq,
                    pss_portion
            );
#elif defined (CPU_THREAD) && defined(CPU_FREQ) 
            fprintf(ofp, "%6.2fs,%4llu.%02llu, %7d, %7d, %7d, %7d, %7d \n",
                    (double)timeStamp/CLOCKS_PER_SEC,
                    usage100/100, usage100%100,
                    c0.run_thread,
                    c0.scaling_cur_freq,
                    c1.scaling_cur_freq,
                    c2.scaling_cur_freq,
                    c3.scaling_cur_freq
            );
#else
            fprintf(ofp, "%6.2fs,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu,%4llu.%02llu\n",
                    (double)timeStamp/CLOCKS_PER_SEC,
                    usage100/100, usage100%100,       
                    u100.user/100, u100.user%100,
                    u100.nice/100, u100.nice%100,				
                    u100.system/100, u100.system%100,

                    u100.idle/100, u100.idle%100,
                    u100.iowait/100, u100.iowait%100,
                    u100.irq/100, u100.irq%100,
                    u100.softirq/100, u100.softirq%100,

                    u100.steal/100, u100.steal%100,
                    u100.guest/100, u100.guest%100,
                    u100.guest_nice/100, u100.guest_nice%100
            );
#endif
		 
            fclose(ofp);

			#ifdef SCORECALC
			RESOURCE_USAGE_T stResourceUsage;
			stResourceUsage.cpuUsage = usage100;
			stResourceUsage.threadUsage = c0.run_thread;
			stResourceUsage.memoryUsage = pss_portion;

			int score = calcResourceScore(&stResourceUsage);
			#endif



        }
        else
        {
            printf("cpu usage logging finished\n");
            return 0;
        }
    }
	
    return 0;
}

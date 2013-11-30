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
//#include <error.h>

#include "scoreCalc.h"
#include "cpuinfo.h"
#include "meminfo.h"

#define INTERVAL 2      /* 5 secs */
#define SCORECALC
 
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
    procstat_t stPreCpuUsage, stCurCpuUsage;
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

#ifdef SCORECALC
	SCORE_RESULT_T stScore = {};
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

    printf("\n v1.3 log Interval = %d[us] log Time = %d[s]\n", logInterval, logTime);
		
    time(&old);
	read_proc_stat(&stPreCpuUsage);
	
    while(1)
    {
        time(&new);
        if(difftime(new, old) < logTime)
        {
			usleep(logInterval);
			
            timeStamp = clock();

            read_proc_stat(&stCurCpuUsage);
            diff_proc_stat(&stCurCpuUsage, &stPreCpuUsage, &d);
			stPreCpuUsage = stCurCpuUsage;

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

                if (mkdir("/data/cpulog", 775) == -1 && errno != EEXIST) {
                    fprintf(stderr, "Problem creating directory");
                    perror(" ");
                }


				#ifdef SCORECALC
				//Load each Usage  & Score value data from Text files.
				loadUsageScoreValue();
				#endif

                ofp = fopen("/data/cpulog/cpulog.txt", "w");
                if(ofp == NULL)
                {
                    printf("fopen w error\n");
                    return 0;
                }

#if defined(CPU_THREAD) && defined(CPU_FREQ) && defined(PROC_MEM) && defined(SCORECALC)
                fprintf(ofp, "[%5.5s],[%5.5s],[%5.5s], [%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s],[%5.5s]\n", "time","cpu","thread", "freq0", "freq1","freq2","freq3", "mem","score", "avgScore");
#elif defined(CPU_THREAD) && defined(CPU_FREQ) && defined(PROC_MEM)
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

            ofp = fopen("/data/cpulog/cpulog.txt", "a");
            if(ofp == NULL)
            {
                printf("fopen a error\n");
                return 0;
            }		

			#ifdef SCORECALC
			RESOURCE_USAGE_T stResourceUsage;
			stResourceUsage.cpuUsage = (float)usage100/100;
			stResourceUsage.threadUsage = c0.run_thread;
			stResourceUsage.memoryUsage = pss_portion;

			stScore = calcResourceScore(&stResourceUsage);
			#endif


#if defined(CPU_THREAD) && defined(CPU_FREQ) && defined(SCORECALC)
            fprintf(ofp, "%6.2fs,%4llu.%02llu, %7d, %7d, %7d, %7d, %7d, %4.2f,%7d,%.2f \n",
                    (double)timeStamp/CLOCKS_PER_SEC,
                    usage100/100, usage100%100,
                    c0.run_thread,
                    c0.scaling_cur_freq,
                    c1.scaling_cur_freq,
                    c2.scaling_cur_freq,
                    c3.scaling_cur_freq,
                    pss_portion,
                    stScore.score,
                    stScore.avgScore
            );

#elif defined(CPU_THREAD) && defined(CPU_FREQ)
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


        }
        else
        {
            printf("cpu usage logging finished\n");
            return 0;
        }
    }
	
    return 0;
}

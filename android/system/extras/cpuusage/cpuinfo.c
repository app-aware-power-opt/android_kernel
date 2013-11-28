

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cpuinfo.h"


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


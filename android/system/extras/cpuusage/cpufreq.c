

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "cpufreq.h"

unsigned int exynos4412_cpufreq_table[] = {1704000, 1600000, 1500000, 1400000, 1300000, 1200000, 1100000, 1000000, 900000, 800000, 700000, 600000, 500000, 400000, 300000, 200000};

unsigned int read_scaling_governor(void)
{
    FILE *fp;
    char scaling_governor_buf[GOVERNOR_LENGTH];
    int ret;
    cpufreq_governor_t result;

    memset(scaling_governor_buf, 0, GOVERNOR_LENGTH);
 
    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
    if(fp == NULL)
    {
       // fprintf(stderr, "open scaling_governor error : %s\n", strerror(errno));
        exit(-1);
    }

    ret = fscanf(fp, "%s", scaling_governor_buf);
    if(ret < 0)
    {
       // fprintf(stderr, "read scaling governor error : %s\n", strerror(errno));
       fclose(fp);
       exit(-1);
    }

    if(strcmp(scaling_governor_buf, "ondemand") == 0)
    {
        result = G_ONDEMAND;
    }
    else if(strcmp(scaling_governor_buf, "userspace") == 0)
    {
        result = G_USERSPACE;
    }
    else if(strcmp(scaling_governor_buf, "performance") == 0)
    {
        result = G_PERFORMANCE;
    }
    else if(strcmp(scaling_governor_buf, "powersave") == 0)
    {
        result = G_POWERSAVE;
    }
    else if(strcmp(scaling_governor_buf, "conservative") == 0)
    {
        result = G_CONSERVATIVE;
    }
    else if(strcmp(scaling_governor_buf, "interactive") == 0)
    {
        result = G_INTERACTIVE;
    }
    else if(strcmp(scaling_governor_buf, "adaptive") == 0)
    {
        result = G_ADAPTIVE;
    }
    else
    {
        fprintf(stderr, "unknown scaling governor : %s\n", scaling_governor_buf);
        fclose(fp);
        exit(-1);
    }
 
    fclose(fp);
    return  result;
}


void set_scaling_governor(unsigned int scaling_governor)
{
    FILE *fp;
    int ret;

    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "w");
    if(fp == NULL)
    {
       // fprintf(stderr, "open scaling_governor error : %s\n", strerror(errno));
        exit(-1);
    }
    
    switch(scaling_governor)
    {
        case G_ONDEMAND : 
            ret = fprintf(fp, "%s", "ondemand");
            break;
        case G_USERSPACE :
            ret = fprintf(fp, "%s", "userspace");
            break;
         case G_PERFORMANCE :
            ret = fprintf(fp, "%s", "performance");
            break;
         case G_POWERSAVE :
            ret = fprintf(fp, "%s", "powersave");
            break;
         case G_CONSERVATIVE :
            ret = fprintf(fp, "%s", "conservative");
            break;
         case G_INTERACTIVE :
            ret = fprintf(fp, "%s", "interactive");
            break;
         case G_ADAPTIVE :
            ret = fprintf(fp, "%s", "adaptive");
            break;
         default : 
            fprintf(stderr, "unknown scaling governor : %d\n", scaling_governor);
            ret = -1;
            break;
    }

    if(ret <= 0)
    {
        fprintf(stderr, "writing scaling governor error : %s\n", strerror(errno));
        fclose(fp);
        exit(-1);
    }

    fclose(fp);
}


void set_cpufreq_to_max(void)
{
    FILE *fp;
    int ret;
    cpufreq_governor_t cur_governor;

    cur_governor = read_scaling_governor();
    if(cur_governor != G_USERSPACE)
    {
        fprintf(stderr, "current governor is : %d, change to userspace(%d)\n", cur_governor, G_USERSPACE);
        set_scaling_governor(G_USERSPACE);
    }

    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed", "w");
    if(fp == NULL)
    {
       // fprintf(stderr, "open scaling_setspeed error : %s\n", strerror(errno));
        exit(-1);
    }

    ret = fprintf(fp, "%d", CPUFREQ_MAX);
    if(ret <= 0)
    {
        fprintf(stderr, "writing scaling_setspeed error : %s\n", strerror(errno));
        fclose(fp);
        exit(-1);
    }

    fclose(fp);
}

void set_cpufreq_to_min(void)
{
    FILE *fp;
    int ret;
    cpufreq_governor_t cur_governor;

    cur_governor = read_scaling_governor();
    if(cur_governor != G_USERSPACE)
    {
        fprintf(stderr, "current governor is : %d, change to userspace(%d)\n", cur_governor, G_USERSPACE);
        set_scaling_governor(G_USERSPACE);
    }

    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed", "w");
    if(fp == NULL)
    {
       // fprintf(stderr, "open scaling_setspeed error : %s\n", strerror(errno));
        exit(-1);
    }

    ret = fprintf(fp, "%d", CPUFREQ_MIN);
    if(ret <= 0)
    {
        fprintf(stderr, "writing scaling_setspeed error : %s\n", strerror(errno));
        fclose(fp);
        exit(-1);
    }

    fclose(fp);
}

void set_cpufreq_to_value(unsigned int freq)
{
    FILE *fp;
    int ret;
    cpufreq_governor_t cur_governor;

    cur_governor = read_scaling_governor();
    if(cur_governor != G_USERSPACE)
    {
        fprintf(stderr, "current governor is : %d, change to userspace(%d)\n", cur_governor, G_USERSPACE);
        set_scaling_governor(G_USERSPACE);
    }

    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed", "w");
    if(fp == NULL)
    {
       // fprintf(stderr, "open scaling_setspeed error : %s\n", strerror(errno));
        exit(-1);
    }

    ret = fprintf(fp, "%d", freq);
    if(ret <= 0)
    {
        fprintf(stderr, "writing scaling_setspeed error : %s\n", strerror(errno));
        fclose(fp);
        exit(-1);
    }

    fclose(fp);
}

unsigned int read_scaling_cur_freq(void)
{
    FILE *fp;
    unsigned int scaling_cur_freq;
    int ret;

    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if(fp == NULL)
    {
       // fprintf(stderr, "open scaling_governor error : %s\n", strerror(errno));
        exit(-1);
    }

    ret = fscanf(fp, "%d", &scaling_cur_freq);
    if(ret < 0)
    {
       // fprintf(stderr, "read scaling governor error : %s\n", strerror(errno));
       fclose(fp);
       exit(-1);
    }
 
    fclose(fp);
    return  scaling_cur_freq;
}

unsigned int get_next_freq(unsigned int cur_freq)
{
    unsigned int next_freq;
    int i;

    if(cur_freq == CPUFREQ_MAX)
    {
        next_freq = CPUFREQ_MAX;
    }
    else
    {
        for(i = 0; i < sizeof(exynos4412_cpufreq_table)/sizeof(unsigned int); i++)
        {
            if(exynos4412_cpufreq_table[i] <= cur_freq)
            {
                break;
            }
        }

        next_freq = exynos4412_cpufreq_table[i-1];
    }

    return next_freq;
}

void set_cpufreq_to_next_step(void)
{
    cpufreq_governor_t cur_governor;
    unsigned int cur_freq, next_freq;

    cur_governor = read_scaling_governor();
    if(cur_governor != G_USERSPACE)
    {
        fprintf(stderr, "current governor is : %d, change to userspace(%d)\n", cur_governor, G_USERSPACE);
        set_scaling_governor(G_USERSPACE);
    }

    cur_freq = read_scaling_cur_freq();
    if(cur_freq == CPUFREQ_MAX)
    {
        fprintf(stderr, "frequency has already been max : %d\n", cur_freq);
        return;
    }

    next_freq = get_next_freq(cur_freq);
    set_cpufreq_to_value(next_freq);
    fprintf(stderr, "change cpu freq from %d to %d\n", cur_freq, next_freq);
}

unsigned int get_prev_freq(unsigned int cur_freq)
{
    unsigned int prev_freq;
    int i;

    if(cur_freq == CPUFREQ_MIN)
    {
        prev_freq = CPUFREQ_MIN;
    }
    else
    {
        for(i = 0; i < sizeof(exynos4412_cpufreq_table)/sizeof(unsigned int); i++)
        {
            if(exynos4412_cpufreq_table[i] <= cur_freq)
            {
                break;
            }
        }

        prev_freq = exynos4412_cpufreq_table[i+1];
    }

    return prev_freq;
}

void set_cpufreq_to_prev_step(void)
{
    cpufreq_governor_t cur_governor;
    unsigned int cur_freq, prev_freq;

    cur_governor = read_scaling_governor();
    if(cur_governor != G_USERSPACE)
    {
        fprintf(stderr, "current governor is : %d, change to userspace(%d)\n", cur_governor, G_USERSPACE);
        set_scaling_governor(G_USERSPACE);
    }

    cur_freq = read_scaling_cur_freq();
    if(cur_freq == CPUFREQ_MIN)
    {
        fprintf(stderr, "frequency has already been min : %d\n", cur_freq);
        return;
    }

    prev_freq = get_prev_freq(cur_freq);
    set_cpufreq_to_value(prev_freq);
    fprintf(stderr, "change cpu freq from %d to %d\n", cur_freq, prev_freq);
}


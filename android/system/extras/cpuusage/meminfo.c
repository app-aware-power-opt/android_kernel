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

#include <fcntl.h>
#include <errno.h>

#ifdef PROC_MEM
#include <dirent.h>
#include <sys/types.h>
#include <pagemap/pagemap.h>
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

int read_use_pss(void)
{
    FILE *fp;
    int use_pss = 0;
    int ret;
 
    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/use_pss", "r");
    if(fp == NULL)
    {
       // fprintf(stderr, "read /proc/stat error : %s\n", strerror(errno));
        exit(-1);
    }
 

    ret = fscanf(fp, "%d", &use_pss);
 
    fclose(fp);

    return use_pss;
}

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
        return 0;
    }

    const int len = read(fd, buffer, sizeof(buffer)-1);
    close(fd);

    if (len < 0) {
        printf("Empty /proc/meminfo");
        return 0;
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
static float meminfo[] = { 0, 0, 0, 0 , 0 }; /*TOTAL, PSS, PSS%, MEM, MEM%*/
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
    int use_pss = 0;
    float ret = 0;

    #define WS_OFF   0
    #define WS_ONLY  1
    #define WS_RESET 2

    use_pss = read_use_pss();

    if(use_pss == 1)
    {
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
    }

    total_mem = print_mem_info();
    if(total_mem <= 0)
    {
        fprintf(stderr, "Error getting available memory\n");
        return -1;
    }

    if(use_pss == 1)
    {
        printf("%5s  %7.2f%%  %7.2f%%  %7.2f%%  %7.2f%%  %s(%7ldK)\n",
                "", (float)total_vss*100/total_mem, (float)total_rss*100/total_mem, (float)total_pss*100/total_mem, (float)total_uss*100/total_mem, "RAM TOTAL", total_mem);
    }

    //printf("%5s  RAM: %ldK total, %ldK free, %ldK buffers, %ldK cached\n\n",
    //        "", mem[0], mem[1], mem[2], mem[3]);

    memset(meminfo, 0, sizeof(meminfo));
    meminfo[0] = (float)total_mem; /*TOTAL*/
    if(use_pss == 1)
    {
        meminfo[1] = (float)total_pss; /*PSS*/
        meminfo[2] = (float)total_pss*100/total_mem; /*PSS %*/
    }
    meminfo[3] = (float)(mem[0] - mem[1] - mem[2] - mem[3]); /*MEM USED*/
    meminfo[4] = (float)(mem[0] - mem[1] - mem[2] - mem[3])*100/total_mem; /*MEM USED %*/

    if(use_pss == 1)
    {
        ret = (float)total_pss*100/total_mem;
    }
    else
   {
       ret = (float)(mem[0] - mem[1] - mem[2] - mem[3])*100/total_mem; 
    }
  
    return ret;
}
#endif
#ifndef _CPU_H
#define _CPU_H

#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#include <stdio.h>

// https://stackoverflow.com/questions/8551418/c-preprocessor-macro-for-returning-a-string-repeated-a-certain-number-of-times
#define PRF_REP0(X)
#define PRF_REP1(X)     X
#define PRF_REP2(X)     PRF_REP1(X) X
#define PRF_REP3(X)     PRF_REP2(X) X
#define PRF_REP4(X)     PRF_REP3(X) X
#define PRF_REP5(X)     PRF_REP4(X) X
#define PRF_REP6(X)     PRF_REP5(X) X
#define PRF_REP7(X)     PRF_REP6(X) X
#define PRF_REP8(X)     PRF_REP7(X) X
#define PRF_REP9(X)     PRF_REP8(X) X
#define PRF_REP10(X)    PRF_REP9(X) X

#define PRF_REP(HUNDREDS,TENS,ONES,X) \
    PRF_REP##HUNDREDS(PRF_REP10(PRF_REP10(X))) \
    PRF_REP##TENS(PRF_REP10(X)) \
    PRF_REP##ONES(X)

#define PRF_TRUE        "true"
#define PRF_FALSE       "false"

typedef enum {
    TYPE_MIN_1          = 1,
    TYPE_MIN_5          = 5,
    TYPE_MIN_15         = 15
} prf_cpu_load_t;

typedef struct prf_perf {
    bool*               is_running;
    bool                is_debug;
    bool                is_joinable;
    const char*         thread_name;
    struct timespec*    sleep_req;
    char*               cpu_name;
    prf_cpu_load_t      cpu_load_type;
    float               cpu_threshold;
    float*              current_threshold;
    const char*         interface_name;
} prf_perf_t;

/*
 * periodically collects CPU and network statistics
 */
void* prf_perf_collect(void* arg);

/*
 * read the system load averages for the past 1, 5, and 15 minutes
 */
bool prf_read_load_avg();

/*
 * prints load averages, for debug purposes
 */
void prf_print_load_avg();

/*
 * fills load averages into array <v>, similar to top
 * v[0] = load averages for the past 1 minute
 * v[1] = load averages for the past 5 minutes
 * v[2] = load averages for the past 15 minutes
 */
void prf_get_load_avg(float v[3]);

/*
 * reports current load average threshold value;
 */
float prf_get_current_load_avg();

/*
 * reports whether the given load average type is valid or not
 */
bool prf_is_valid_load_avg_val(int val);

/*
 * The canonical source of this information is linux src documentation, f.e.:
 * /usr/src/linux/Documentation/filesystems/proc.txt
 * On http://git.kernel.org:
 * http://git.kernel.org/?p=linux/kernel/git/torvalds/linux-2.6.git;a=blob;f=Documentation/filesystems/proc.txt;hb=HEAD#l451
 *
 * reads cpu line of /proc/stat
 * http://procps.sourceforge.net/index.html
 */
bool prf_read_cpu_info();

/*
 * prints CPU values, for debug purposes
 */
void prf_print_cpu_load();

/*
 * prints CPU percentages, for debug purposes
 */
void prf_print_cpu_pt_load();

/*
 * fills raw CPU data into array <c>, similar to /proc/stat's first line
 * c[0] = user: normal processes executing in user mode
 * c[1] = system: processes executing in kernel mode
 * c[2] = nice: niced processes executing in user mode
 * c[3] = idle: twiddling thumbs
 * c[4] = iowait: waiting for I/O to complete
 * c[5] = irq: servicing interrupts (hardware interrupts)
 * c[6] = softirq: servicing softirqs (software interrupts)
 * c[7] = steal: involuntary wait
 */
void prf_get_cpu_raw_info(unsigned long c[8]);

/*
 * fills percentage of CPU time spent in each eight cats into array <p>, similar to top
 * p[0] = %us, user: normal processes executing in user mode
 * p[1] = %sy, system: processes executing in kernel mode
 * p[2] = %ni, nice: niced processes executing in user mode
 * p[3] = %id, idle: twiddling thumbs
 * p[4] = %wa, iowait: waiting for I/O to complete
 * p[5] = %hi, irq: servicing interrupts (hardware interrupts)
 * p[6] = %si, softirq: servicing softirqs (software interrupts)
 * p[7] = %st, steal: involuntary wait
 */
void prf_get_cpu_pt_info(float p[8]);

/*
 *  returns CPU idle percentage
 */
float prf_get_cpu_idle();

/*
 *  returns CPU load percentage
 */
float prf_get_cpu_load();

/*
 * parses /proc/meminfo
 * http://procps.sourceforge.net/index.html
 */
bool  prf_read_mem_info();

/*
 * prints a summary for mem info, for debug purposes
 */
void prf_print_mem_info();

/*
 * prints all mem info, for debug purposes
 */
void prf_print_mem_info_full();

/*
 * required for bsearch() call in prf_read_mem_info()
 */
int prf_compare_mem_table_structs(const void* a, const void* b);

/*
 * reads momory data and fills it into array <m>, similar to top
 * m[0] = mem total
 * m[1] = mem used
 * m[2] = mem free
 * m[3] = mem buffers
 * m[4] = swap total
 * m[5] = swap used
 * m[6] = swap free
 * m[7] = swap cached
 */
void prf_get_current_mem_info(unsigned long m[8]);

/*
 * parses /proc/net/dev
 */
bool prf_read_net_info();

/*
 * prints network info, similar to /proc/net/dev
 */
void prf_print_net_info();

/*
 * prints network interface's Rx and Tx rates (kbps), for debug purposes
 */
void prf_print_net_rates();

/*
 * fills raw network Rx info into array <r>
 * r[0] = Rx bytes
 * r[1] = Rx packets
 * r[2] = Rx errs
 * r[3] = Rx drop
 * r[4] = Rx fifo
 * r[5] = Rx frame
 * r[6] = Rx compressed
 * r[7] = Rx multicast
 * fills raw network Tx info into array <t>
 * t[0] = Tx bytes
 * t[1] = Tx packets
 * t[2] = Tx errs
 * t[3] = Tx drop
 * t[4] = Tx fifo
 * t[5] = Tx colls
 * t[6] = Tx carrier
 * t[7] = Tx compressed
 */
void prf_get_net_raw_info(unsigned long r[8], unsigned long t[8]);

/*
 * fills Rx and Tx rates into array <n>
 * n[0] = Rx rate (kb/s)
 * n[1] = Tx rate (kb/s)
 */
void prf_get_net_rate_info(float n[2]);

/*
 * reads file <file_name> into <buffer>
 * if <*file_size> == 0 then it finds out the size of the file itself
 * <*file_size> should be sized for the extra '\0' char
 * if <*buffer> == NULL then it allocates memory itself
 */
bool prf_read_file(const char* file_name, char** buffer, long* file_size);

/*
 * frees heap memory
 */
void prf_free_mem(void* mem);

/*
 * reports whether thread for periodic performance measurements is running or not
 */
bool prf_is_perf_thread_running();

/*
 * cancels thread for periodic performance measurements
 */
void prf_cancel_perf_thread();

#endif /* _CPU_H */

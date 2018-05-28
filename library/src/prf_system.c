#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include "prf_system.h"

#define PRF_PRINT_ERROR

#define PRF_LIB_HEADER          PRF_REP(0,8,0, "-")
#define PRF_PER_READS           "PERIODIC READS"
#define PRF_LOAD_AVG_FILE       "/proc/loadavg"
#define PRF_CPU_INFO_FILE       "/proc/stat"
#define PRF_MEM_INFO_FILE       "/proc/meminfo"
#define PRF_NET_INFO_FILE       "/proc/net/dev"
#define PRF_READ_FILE           "READ: %s\n"
#define PRF_MEM_INFO_LINE       "%-16s%12ld kB\n"
#define PRF_AVG_BUFF_SIZE       256
#define PRF_CPU_BUFF_SIZE       4096
#define PRF_MEM_BUFF_SIZE       4096
#define PRF_NET_BUFF_SIZE       4096
#define PRF_NET_UNIT_CONV       0.008   // 1 byte = 8 bit, 1 kilo = 1000 : a kilobit = b * 8 / 1000  bytes
#define PRF_MEM_ARRAY_LEN       48
#define PRF_CPU_ARRAY_LEN       8
#define PRF_NET_ARRAY_LEN       8

typedef struct mem_name_value {
    const char*                 name;   // memory type name
    unsigned long*              slot;   // slot in return struct
} mem_name_value_t;

// running?
static bool*                    prf_perf_is_running;
// CFG: debug
static bool                     prf_cfg_is_debug;
// CFG: joinable
static bool                     prf_cfg_is_joinable;
// CFG: thread name
static char*                    prf_thread_name;
// CFG: interval
static float                    prf_interval_seconds;
// CFG: CPU index: cpu, cpu1, cpu2, ...
static char*                    prf_cfg_cpu_name;
// CFG: CPU load type: 1 | 5 | 15 indicating N min load averages
static prf_cpu_load_t           prf_cfg_cpu_load_type;
// CFG: CPU load average theshold
static float                    prf_cfg_cpu_threshold;
// current threshold
static float*                   prf_perf_current_threshold;
// CFG: network interface name
static char*                    prf_cfg_interface_name;
// load averages
static float                    prf_load_avg[3];
// CPU
static unsigned long            prf_cpu[PRF_CPU_ARRAY_LEN];
static float                    prf_cpu_pt[PRF_CPU_ARRAY_LEN];

// memory
static unsigned long            prf_kb_active;
static unsigned long            prf_kb_anon_pages;
static unsigned long            prf_kb_bounce;
static unsigned long            prf_kb_main_buffers;
static unsigned long            prf_kb_main_cached;
static unsigned long            prf_kb_commit_limit;
static unsigned long            prf_kb_committed_as;
static unsigned long            prf_kb_dirty;
static unsigned long            prf_kb_inactive;
static unsigned long            prf_kb_mapped;
static unsigned long            prf_kb_main_free;
static unsigned long            prf_kb_main_total;
static unsigned long            prf_kb_nfs_unstable;
static unsigned long            prf_kb_pagetables;
static unsigned long            prf_kb_swap_reclaimable;
static unsigned long            prf_kb_swap_unreclaimable;
static unsigned long            prf_kb_slab;
static unsigned long            prf_kb_swap_cached;
static unsigned long            prf_kb_swap_free;
static unsigned long            prf_kb_swap_total;
static unsigned long            prf_kb_vmalloc_chunk;
static unsigned long            prf_kb_vmalloc_total;
static unsigned long            prf_kb_vmalloc_used;
static unsigned long            prf_kb_writeback;
static unsigned long            prf_kb_swap_used; // derived value
static unsigned long            prf_kb_main_used; // derived value

// network
static unsigned long            prf_net_rx[PRF_NET_ARRAY_LEN];
static unsigned long            prf_net_tx[PRF_NET_ARRAY_LEN];
static float                    prf_net_rx_rate;
static float                    prf_net_tx_rate;
static bool                     prf_cpu_warned = false;
static bool                     prf_net_warned = false;

/*
 * thread for collecting CPU and network statistics
 */
void* prf_perf_collect(void* arg) {
    prf_perf_t*                 prf_perf = (prf_perf_t*)arg;
    struct timespec             wait_act;

    // read args
    prf_perf_is_running         = prf_perf->is_running;
    prf_cfg_is_debug            = prf_perf->is_debug;
    prf_cfg_is_joinable         = prf_perf->is_joinable;
    prf_thread_name             = strdup(prf_perf->thread_name);
    prf_interval_seconds        = (float)prf_perf->sleep_req->tv_sec +
                                  ((float)(prf_perf->sleep_req->tv_nsec) / 1000000000.0);
    prf_cfg_cpu_name            = prf_perf->cpu_name;
    prf_cfg_cpu_load_type       = prf_perf->cpu_load_type;
    prf_cfg_cpu_threshold       = prf_perf->cpu_threshold;
    prf_perf_current_threshold  = prf_perf->current_threshold;
    prf_cfg_interface_name      = strdup(prf_perf->interface_name);

    // init
    prf_read_load_avg();
    prf_read_cpu_info();
    prf_read_mem_info();
    prf_read_net_info();

    if (prf_cfg_is_debug) {
        printf("INTERVAL: %6.4fs\n", prf_interval_seconds);
        printf("%s\n", PRF_LIB_HEADER);

        prf_print_load_avg();
        prf_print_cpu_load();
        prf_print_mem_info_full();
        prf_print_net_info();
        printf("-- %-74s --\n%s\n", PRF_PER_READS, PRF_LIB_HEADER);
    }

    while (*prf_perf_is_running) {
        nanosleep(prf_perf->sleep_req, &wait_act);

        prf_read_load_avg();

        if (prf_cfg_is_debug) {
            prf_read_cpu_info();
            prf_read_net_info();
            prf_read_mem_info();

            prf_print_load_avg();
            prf_print_cpu_pt_load();
            prf_print_mem_info();
            prf_print_net_rates();
        }

        if (prf_cfg_is_joinable) {
            float current_threshold = prf_get_current_load_avg();
            printf("-- joined: %4.2f | is overloaded? %s\n",
                   current_threshold, (current_threshold >= prf_cfg_cpu_threshold) ? PRF_TRUE : PRF_FALSE);
        } else {
            *prf_perf_current_threshold = prf_get_current_load_avg();
        }

        if (prf_cfg_is_debug) {
            printf("%s\n", PRF_LIB_HEADER);
        }
    }

    return NULL;
}

bool prf_read_load_avg() {
    bool    status = false;
    long    size   = PRF_AVG_BUFF_SIZE;
    char    buff[size];
    char*   p_buff = buff;

    prf_load_avg[0] = prf_load_avg[1] = prf_load_avg[2] = 0.0;

    if (prf_read_file(PRF_LOAD_AVG_FILE, &p_buff, &size)) {
        sscanf(buff, "%f %f %f",
                      &prf_load_avg[0], &prf_load_avg[1], &prf_load_avg[2]);
        status = true;
    }

    return status;
}

void prf_print_load_avg() {
    printf("READ: %s\n\
Load average: %4.2f, %4.2f, %4.2f\n%s\n",
           PRF_LOAD_AVG_FILE,
           prf_load_avg[0], prf_load_avg[1], prf_load_avg[2],
           PRF_LIB_HEADER);
}

void prf_get_load_avg(float v[3]) {
    memcpy(v, prf_load_avg, sizeof(prf_load_avg));
}

float prf_get_current_load_avg() {
    float   threshold;
    float   v[3];

    prf_get_load_avg(v);

    switch (prf_cfg_cpu_load_type) {
        case TYPE_MIN_1:
            threshold = v[0];
            break;

        case TYPE_MIN_5:
            threshold = v[1];
            break;

        case TYPE_MIN_15:
            threshold = v[2];
            break;

        default:
            threshold = v[1];
            break;
    }

    return threshold;
}

bool prf_is_valid_load_avg_val(int val) {
    switch (val) {
        case TYPE_MIN_1:
        case TYPE_MIN_5:
        case TYPE_MIN_15:
            return true;
            break;

        default:
            return false;
            break;
    }
}

bool prf_read_cpu_info() {
#define TRIMz(x)  ((tz = (long)(x)) < 0 ? 0 : tz)

    bool            status      = false;
    long            size        = PRF_CPU_BUFF_SIZE;
    char            buff[size];
    char*           p_buff      = buff;
    unsigned long   u_frme      = 0;
    unsigned long   s_frme      = 0;
    unsigned long   n_frme      = 0;
    unsigned long   i_frme      = 0;
    unsigned long   w_frme      = 0;
    unsigned long   x_frme      = 0;
    unsigned long   y_frme      = 0;
    unsigned long   z_frme      = 0;
    unsigned long   tot_frme    = 0;
    unsigned long   tz          = 0;
    float           scale       = 0.0;
    unsigned long   cpu_new[PRF_CPU_ARRAY_LEN];
    char*           cpu;

    if (prf_read_file(PRF_CPU_INFO_FILE, &p_buff, &size)) {
        cpu = strstr(buff, prf_cfg_cpu_name);
        if (cpu) {
            // advance the size of the CPU
            cpu += strlen(prf_cfg_cpu_name);

            sscanf(cpu, "%lu %lu %lu %lu %lu %lu %lu %lu",
                         &cpu_new[0], &cpu_new[1], &cpu_new[2], &cpu_new[3],
                         &cpu_new[4], &cpu_new[5], &cpu_new[6], &cpu_new[7]);

            // calculate CPU load
            u_frme = cpu_new[0] - prf_cpu[0];
            s_frme = cpu_new[1] - prf_cpu[1];
            n_frme = cpu_new[2] - prf_cpu[2];
            i_frme = TRIMz(cpu_new[3] - prf_cpu[3]);
            w_frme = cpu_new[4] - prf_cpu[4];
            x_frme = cpu_new[5] - prf_cpu[5];
            y_frme = cpu_new[6] - prf_cpu[6];
            z_frme = cpu_new[7] - prf_cpu[7];

            tot_frme = u_frme + s_frme + n_frme + i_frme + w_frme + x_frme + y_frme + z_frme;

            if (tot_frme < 1) {
                tot_frme = 1;
            }

            scale = 100.0 / (float)tot_frme;

            prf_cpu_pt[0] = (float)u_frme * scale;
            prf_cpu_pt[1] = (float)n_frme * scale;
            prf_cpu_pt[2] = (float)s_frme * scale;
            prf_cpu_pt[3] = (float)i_frme * scale;
            prf_cpu_pt[4] = (float)w_frme * scale;
            prf_cpu_pt[5] = (float)x_frme * scale;
            prf_cpu_pt[6] = (float)y_frme * scale;
            prf_cpu_pt[7] = (float)z_frme * scale;

            // store last read values
            memcpy(prf_cpu, cpu_new, sizeof(cpu_new));

            status = true;
        } else {
#ifdef PRF_PRINT_ERROR
            if (!prf_cpu_warned) {
                prf_cpu_warned = true;
                fprintf(stderr, "** ERROR - unable to find the CPU '%s'\n", prf_cfg_cpu_name);
            }
#endif
        }
    }

    return status;
}

void prf_print_cpu_load() {
    printf("READ: %s\n%s: %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld\n\%s\n",
            PRF_CPU_INFO_FILE,
            prf_cfg_cpu_name,
            prf_cpu[0], prf_cpu[1], prf_cpu[2], prf_cpu[3],
            prf_cpu[4], prf_cpu[5], prf_cpu[6], prf_cpu[7],
            PRF_LIB_HEADER);
}

// modelled after top's CPU line, example:
// Cpu(s):  1.3%us, 28.0%sy,  0.0%ni, 70.3%id,  0.0%wa,  0.3%hi,  0.0%si,  0.0%st
void prf_print_cpu_pt_load() {
    printf("READ: %s\nCpu: %6.1f%%us, %6.1f%%sy, %6.1f%%ni, %6.1f%%id, %6.1f%%wa, %6.1f%%hi, %6.1f%%si, %6.1f%%st\n\%s\n",
            PRF_CPU_INFO_FILE,
            prf_cpu_pt[0], prf_cpu_pt[1], prf_cpu_pt[2], prf_cpu_pt[3],
            prf_cpu_pt[4], prf_cpu_pt[5], prf_cpu_pt[6], prf_cpu_pt[7],
            PRF_LIB_HEADER);
}

void prf_get_cpu_raw_info(unsigned long c[PRF_CPU_ARRAY_LEN]) {
    memcpy(c, prf_cpu, sizeof(prf_cpu));
}

void prf_get_cpu_pt_info(float p[PRF_CPU_ARRAY_LEN]) {
    memcpy(p, prf_cpu_pt, sizeof(prf_cpu_pt));
}

float prf_get_cpu_idle() {
   return prf_cpu_pt[3];
}

float prf_get_cpu_load() {
   return (100.0 - prf_cpu_pt[3]);
}

bool prf_read_mem_info() {
    bool                    status          = false;
    char*                   namebuf;
    mem_name_value_t        find_me;
    mem_name_value_t*       found;
    long                    size            = PRF_MEM_BUFF_SIZE;
    char                    buff[size];
    char*                   p_buff          = buff;
    char*                   head;
    char*                   delim;
    int                     check           = false;
    char*                   delim_line      = "\n";
    char*                   delim_param     = ":";
    // https://www.gnu.org/software/libc/manual/html_node/Array-Search-Function.html
    // The bsearch function searches the sorted array <array> for an object that is equivalent to <key>.
    mem_name_value_t        mem_data[]      = {
                                                {"Active",          &prf_kb_active},
                                                {"AnonPages",       &prf_kb_anon_pages},
                                                {"Bounce",          &prf_kb_bounce},
                                                {"Buffers",         &prf_kb_main_buffers},
                                                {"Cached",          &prf_kb_main_cached},
                                                {"CommitLimit",     &prf_kb_commit_limit},
                                                {"Committed_AS",    &prf_kb_committed_as},
                                                {"Dirty",           &prf_kb_dirty},
                                                {"Inactive",        &prf_kb_inactive},
                                                {"Mapped",          &prf_kb_mapped},
                                                {"MemFree",         &prf_kb_main_free},
                                                {"MemTotal",        &prf_kb_main_total},
                                                {"NFS_Unstable",    &prf_kb_nfs_unstable},
                                                {"PageTables",      &prf_kb_pagetables},
                                                {"SReclaimable",    &prf_kb_swap_reclaimable},
                                                {"SUnreclaim",      &prf_kb_swap_unreclaimable},
                                                {"Slab",            &prf_kb_slab},
                                                {"SwapCached",      &prf_kb_swap_cached},
                                                {"SwapFree",        &prf_kb_swap_free},
                                                {"SwapTotal",       &prf_kb_swap_total},
                                                {"VmallocChunk",    &prf_kb_vmalloc_chunk},
                                                {"VmallocTotal",    &prf_kb_vmalloc_total},
                                                {"VmallocUsed",     &prf_kb_vmalloc_used},
                                                {"Writeback",       &prf_kb_writeback}
                                           };

    if (prf_read_file(PRF_MEM_INFO_FILE, &p_buff, &size)) {
        char* rest_line = NULL;
        for (char* line = strtok_r(buff, delim_line, &rest_line);
            line != NULL;
            line = strtok_r(NULL, delim_line, &rest_line)) {
            char* p_delim   = strstr(line, delim_param);
            // turn string <line> into <name> and <value> strings
            *p_delim        = '\0';
            char* p_name    = line;
            char* p_value   = p_delim + 1;

            find_me.name    = p_name;
            found           = (mem_name_value_t*)bsearch(&find_me, mem_data, sizeof(mem_data) / sizeof(mem_name_value_t),
                                                        sizeof(mem_data[0]), prf_compare_mem_table_structs);

            if (found) {
                *(found->slot) = strtoull(p_value, NULL, 10);
            }
        }

        // derived
        prf_kb_swap_used = prf_kb_swap_total - prf_kb_swap_free;
        prf_kb_main_used = prf_kb_main_total - prf_kb_main_free;

        p_buff = NULL;

        status = true;
    }

    return status;
}

// modelled after top's memory lines, example:
// Mem:    767684k total,   748792k used,    18892k free,   108260k buffers
// Swap:   831480k total,        0k used,   831480k free,   294848k cached
void prf_print_mem_info() {
    printf("READ: %s\n\
Mem: %9ldk total, %8ldk used, %8ldk free, %8ldk buffers\n\
Swap: %8ldk total, %8ldk used, %8ldk free, %8ldk cached\n\%s\n",
        PRF_MEM_INFO_FILE,
        prf_kb_main_total, prf_kb_main_used, prf_kb_main_free, prf_kb_main_buffers,
        prf_kb_swap_total, prf_kb_swap_used, prf_kb_swap_free, prf_kb_main_cached,
        PRF_LIB_HEADER);
}

void prf_print_mem_info_full() {
    printf("READ: %s\n",        PRF_MEM_INFO_FILE);
    printf(PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE \
           PRF_MEM_INFO_LINE,
           "Active:",          prf_kb_active,
           "AnonPages:",       prf_kb_anon_pages,
           "Bounce:",          prf_kb_bounce,
           "Buffers:",         prf_kb_main_buffers,
           "Cached:",          prf_kb_main_cached,
           "CommitLimit:",     prf_kb_commit_limit,
           "Committed_AS:",    prf_kb_committed_as,
           "Dirty:",           prf_kb_dirty,
           "Inactive:",        prf_kb_inactive,
           "Mapped:",          prf_kb_mapped,
           "MemFree:",         prf_kb_main_free,
           "MemTotal:",        prf_kb_main_total,
           "NFS_Unstable:",    prf_kb_nfs_unstable,
           "PageTables:",      prf_kb_pagetables,
           "SReclaimable:",    prf_kb_swap_reclaimable,
           "SUnreclaim:",      prf_kb_swap_unreclaimable,
           "Slab:",            prf_kb_slab,
           "SwapCached:",      prf_kb_swap_cached,
           "SwapFree:",        prf_kb_swap_free,
           "SwapTotal:",       prf_kb_swap_total,
           "VmallocChunk:",    prf_kb_vmalloc_chunk,
           "VmallocTotal:",    prf_kb_vmalloc_total,
           "VmallocUsed:",     prf_kb_vmalloc_used,
           "Writeback:",       prf_kb_writeback);
    printf("%s\n",      PRF_LIB_HEADER);
}

int prf_compare_mem_table_structs(const void* a, const void* b) {
    return strcmp(((const mem_name_value_t*)a)->name, ((const mem_name_value_t*)b)->name);
}

void prf_get_current_mem_info(unsigned long m[8]) {
    m[0] = prf_kb_main_total;
    m[1] = prf_kb_main_used;
    m[2] = prf_kb_main_free;
    m[3] = prf_kb_main_buffers;
    m[4] = prf_kb_swap_total;
    m[5] = prf_kb_swap_used;
    m[6] = prf_kb_swap_free;
    m[7] = prf_kb_main_cached;
}

bool prf_read_net_info() {
    bool                    status          = false;
    long                    size            = PRF_NET_BUFF_SIZE;
    char                    buff[size];
    char*                   p_buff          = buff;
    char*                   eth;
    unsigned long           net_new_rx[PRF_NET_ARRAY_LEN];
    unsigned long           net_new_tx[PRF_NET_ARRAY_LEN];

    if (prf_read_file(PRF_NET_INFO_FILE, &p_buff, &size)) {
        eth = strstr(buff, prf_cfg_interface_name);
        if (eth) {
            // advance the size of the interface
            eth += strlen(prf_cfg_interface_name);

            sscanf(eth, ": %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                         &net_new_rx[0], &net_new_rx[1], &net_new_rx[2], &net_new_rx[3],
                         &net_new_rx[4], &net_new_rx[5], &net_new_rx[6], &net_new_rx[7],
                         &net_new_tx[0], &net_new_tx[1], &net_new_tx[2], &net_new_tx[3],
                         &net_new_tx[4], &net_new_tx[5], &net_new_tx[6], &net_new_tx[7]);

            if (prf_interval_seconds > 0.0) {
                prf_net_rx_rate = (float)(((net_new_rx[0] - prf_net_rx[0]) / prf_interval_seconds) * PRF_NET_UNIT_CONV);
                prf_net_tx_rate = (float)(((net_new_tx[0] - prf_net_tx[0]) / prf_interval_seconds) * PRF_NET_UNIT_CONV);
            } else {
                prf_net_rx_rate = 0.0;
                prf_net_tx_rate = 0.0;
            }

            // store last read values
            memcpy(prf_net_rx, net_new_rx, sizeof(net_new_rx));
            memcpy(prf_net_tx, net_new_tx, sizeof(net_new_tx));

            p_buff  = NULL;
            eth     = NULL;

            status = true;
        } else {
#ifdef PRF_PRINT_ERROR
            if (!prf_net_warned) {
                prf_net_warned = true;
                fprintf(stderr, "** ERROR - unable to find the interface '%s'\n", prf_cfg_interface_name);
            }
#endif
        }
    }

    return status;
}

// modelled after /proc/net/dev, example:
//Inter-|   Receive                                                |  Transmit
// face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
//    lo:     720      12    0    0    0     0          0         0      720      12    0    0    0     0       0          0
//  eth0:   11031      34    0    0    0     0          0         0     5996      48    0    0    0     0       0          0
void prf_print_net_info() {
    printf("READ: %s\n\
Interface | Receive                                                        | Transmit\n\
          | bytes\tpackets\terrs\tdrop\tfifo\tframe\tcompressed\tmulticast | bytes\tpackets\terrs\tdrop\tfifo\tcolls\tcarrier\tcompressed\n\
%s %8lu\t%8lu\t%5lu\t%5lu\t%5lu\t%6lu\t%11lu\t%10lu | %8lu\t%8lu\t%5lu\t%5lu\t%5lu\t%6lu\t%8lu\t%11lu\n%s\n",
           PRF_NET_INFO_FILE,
           prf_cfg_interface_name,
           prf_net_rx[0], prf_net_rx[1], prf_net_rx[2], prf_net_rx[3],
           prf_net_rx[4], prf_net_rx[5], prf_net_rx[6], prf_net_rx[7],
           prf_net_tx[0], prf_net_tx[1], prf_net_tx[2], prf_net_tx[3],
           prf_net_tx[4], prf_net_tx[5], prf_net_tx[6], prf_net_tx[7],
           PRF_LIB_HEADER
    );
}

// Rx and Tx rates (kb/s)
void prf_print_net_rates() {
    printf("READ: %s\nRx: %10lu bytes, %8.2f kbps | Tx: %10lu bytes, %8.2f kbps\n%s\n",
            PRF_NET_INFO_FILE,
            prf_net_rx[0], prf_net_rx_rate, prf_net_tx[0], prf_net_tx_rate,
            PRF_LIB_HEADER);
}

void prf_get_net_raw_info(unsigned long r[PRF_NET_ARRAY_LEN], unsigned long t[PRF_NET_ARRAY_LEN]) {
    memcpy(r, prf_net_rx, sizeof(prf_net_rx));
    memcpy(t, prf_net_tx, sizeof(prf_net_tx));
}

void prf_get_net_rate_info(float n[2]) {
    n[0] = prf_net_rx_rate;
    n[1] = prf_net_tx_rate;
}

bool prf_read_file(const char* file_name, char** buffer, long* file_size) {
    bool        status = false;
    FILE*       fl;

    fl = fopen(file_name, "rb");

    if (fl && file_size) {
        // get file size
        if (*file_size == 0) {
            fseek(fl, 0, SEEK_END);
            *file_size = ftell(fl);
            fseek(fl, 0, SEEK_SET);
            (*file_size)++; // +1 for '\0'
        }

        if (*buffer == NULL) {
            *buffer = (char*)malloc(*file_size);
        }

        if (*buffer) {
            memset(*buffer, 0, *file_size);
            fread(*buffer, 1, (*file_size -1), fl);
            status = true;
        } else {
#ifdef PRF_PRINT_ERROR
            fprintf(stderr, "** ERROR - memory error!");
#endif
        }

        fclose(fl);
    } else {
#ifdef PRF_PRINT_ERROR
            fprintf(stderr, "** ERROR - unable to open file '%s'\n", file_name);
#endif
    }

    return status;
}

void prf_free_mem(void* mem) {
    if (mem != NULL) {
        free(mem);
        mem = NULL;
    }
}

bool prf_is_perf_thread_running() {
    return *prf_perf_is_running;
}

void prf_cancel_perf_thread() {
    if (*prf_perf_is_running) {
        *prf_perf_is_running = false;
    }
}


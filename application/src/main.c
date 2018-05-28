// _GNU_SOURCE is required for 'pthread_setname_np'
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>

#include "prf_system.h"

#define PRF_APP_HEADER          PRF_REP(0,8,0, "=")
#define PRF_APP_LABEL           "PERFORMANCE MEASUREMENTS"
#define PRF_CONFIG_FILE         "prf_system.cfg"
#define PRF_DEF_IS_DEBUG        true
#define PRF_DEF_IS_JOINABLE     true
#define PRF_DEF_THREAD_NAME     "prf_thread"
#define PRF_DEF_INTERVAL_S      3
#define PRF_DEF_INTERVAL_MS     0
#define PRF_DEF_CPU_NAME       "cpu"
#define PRF_DEF_CPU_LOAD_TYPE   5
#define PRF_DEF_CPU_THRESHOLD   0.70
#define PRF_DEF_NET_ITF_NAME    "wlp2s0"

// for signal_handler()
static pthread_t    prf_thread_ext;
static void         prf_signal_handler(int signal);
// read-write bi-directional params of the thread
static bool         is_running              = true;
static float        current_cpu_threshold   = 0.0;

// config
typedef struct config {
    bool            is_debug;
    bool            is_joinable;
    char*           thread_name;
    int             interval_s;
    int             interval_ms;
    char*           cpu_name;
    prf_cpu_load_t  cpu_load_type;
    float           cpu_threshold;
    char*           interface_name;
} config_t;

bool is_line_comment(char* line, char* delim) {
    return (strncmp(delim, line, strlen(delim)) == 0);
}

bool is_equal(char* name, char* name_val) {
    return (strcmp(name, name_val) == 0);
}

// 'cfg' is filled with defaults
void read_config(char* file_cfg, config_t* cfg, bool debug) {
    char*       cfg_names[]             = {"is_debug",
                                           "is_joinable",
                                           "thread_name",
                                           "interval_s",
                                           "interval_ms",
                                           "cpu_name",
                                           "cpu_load_type",
                                           "cpu_threshold",
                                           "interface_name"};
    char*       cfg_start_comment       = "#";
    char*       cfg_delim_line          = "\n";
    char*       cfg_delim_param         = "=";
    long        size                    = 2048;
    char        buff[size];
    char*       p_buff = buff;

    if (debug) {
        printf("CONFIGURATION:\n");
    }

    if (prf_read_file(file_cfg, &p_buff, &size)) {
        char* rest_line = NULL;
        for (char* line = strtok_r(buff, cfg_delim_line, &rest_line);
            line != NULL;
            line = strtok_r(NULL, cfg_delim_line, &rest_line)) {
            if (!is_line_comment(line, cfg_start_comment)) {
                char* p_delim   = strstr(line, cfg_delim_param);
                // turn string <line> into <name> and <value> strings
                *p_delim        = '\0';
                char* p_name    = line;
                char* p_value   = p_delim + 1;

                //printf("READ: '%s' = '%s'\n", p_name, p_value);

                if (is_equal(p_name, cfg_names[0]))  {
                    cfg->is_debug = is_equal(p_value, "true");
                } else if (is_equal(p_name, cfg_names[1]))  {
                    cfg->is_joinable = is_equal(p_value, "true");
                } else if (is_equal(p_name, cfg_names[2]))  {
                    cfg->thread_name = strdup(p_value);
                } else if (is_equal(p_name, cfg_names[3]))  {
                    cfg->interval_s = strtol(p_value, NULL, 10);
                } else if (is_equal(p_name, cfg_names[4]))  {
                    cfg->interval_ms = strtol(p_value, NULL, 10);
                } else if (is_equal(p_name, cfg_names[5]))  {
                    cfg->cpu_name = strdup(p_value);
                } else if (is_equal(p_name, cfg_names[6]))  {
                    cfg->cpu_load_type = strtol(p_value, NULL, 10);
                    if (!prf_is_valid_load_avg_val(cfg->cpu_load_type)) {
                        printf("** WARNING - invalid CPU load type: '%d' - defaulted to '%d'\n",
                               cfg->cpu_load_type, PRF_DEF_CPU_LOAD_TYPE);
                        cfg->cpu_load_type = PRF_DEF_CPU_LOAD_TYPE;
                    }
                } else if (is_equal(p_name, cfg_names[7]))  {
                    cfg->cpu_threshold = strtof(p_value, NULL);
                } else if (is_equal(p_name, cfg_names[8]))  {
                    cfg->interface_name = strdup(p_value);
                } else {
                    printf("** WARNING - unknown config parameter: '%s' = '%s'\n", p_name, p_value);
                }
            }
        }
    } else {
        printf("** WARNING - reverted to defaults");
    }

    if (debug) {
        printf("is_debug = %s\n", cfg->is_debug ? PRF_TRUE : PRF_FALSE);
        printf("is_joinable = %s\n", cfg->is_joinable ? PRF_TRUE : PRF_FALSE);
        printf("thread_name = %s\n", cfg->thread_name);
        printf("interval_s = %d\n", cfg->interval_s);
        printf("interval_ms = %d\n", cfg->interval_ms);
        printf("cpu_name = %s\n", cfg->cpu_name);
        printf("cpu_load_type = %d\n", cfg->cpu_load_type);
        printf("cpu_threshold = %f4.2\n", cfg->cpu_threshold);
        printf("interface_name = %s\n\n", cfg->interface_name);
    }
}

int main(int argc, char** argv) {
    time_t                      now                             = time(NULL);
    struct tm*                  now_info                        = localtime(&now);
    config_t                    cfg                             = {PRF_DEF_IS_DEBUG,
                                                                   PRF_DEF_IS_JOINABLE,
                                                                   PRF_DEF_THREAD_NAME,
                                                                   PRF_DEF_INTERVAL_S,
                                                                   PRF_DEF_INTERVAL_MS,
                                                                   PRF_DEF_CPU_NAME,
                                                                   PRF_DEF_CPU_LOAD_TYPE,
                                                                   PRF_DEF_CPU_THRESHOLD,
                                                                   PRF_DEF_NET_ITF_NAME};
    float                       current_threshold               = 0.0;
    bool                        create_failed                   = false;
    bool                        join_failed                     = false;
    bool                        setname_failed                  = false;
    bool                        status                          = false;
    int                         delay_ms                        = 200;
    int                         detachstate;
    char*                       tail;
    pthread_attr_t              attr_perf;
    struct timespec             sleep_req;
    char                        now_out[19];
    prf_perf_t                  param_perf;

    signal(SIGINT,  prf_signal_handler);
    signal(SIGTERM, prf_signal_handler);
    signal(SIGQUIT, prf_signal_handler);
    signal(SIGTSTP, prf_signal_handler);

    sprintf(now_out, "%d-%02d-%02d %02d:%02d:%02d",
                     now_info->tm_year + 1900, now_info->tm_mon + 1, now_info->tm_mday,
                     now_info->tm_hour, now_info->tm_min, now_info->tm_sec);

    printf("%s\n", PRF_APP_HEADER);
    printf("== %-74s ==\n", PRF_APP_LABEL);
    printf("== %-74s ==\n", now_out);
    printf("%s\n\n", PRF_APP_HEADER);

    read_config(PRF_CONFIG_FILE, &cfg, true);

    printf("%s\n", PRF_APP_HEADER);

    sleep_req.tv_sec                = (long)cfg.interval_s;
    sleep_req.tv_nsec               = (long)cfg.interval_ms * 1000000L;

    param_perf.is_running           = &is_running,
    param_perf.is_debug             = cfg.is_debug,
    param_perf.is_joinable          = cfg.is_joinable,
    param_perf.thread_name          = cfg.thread_name,
    param_perf.sleep_req            = &sleep_req,
    param_perf.cpu_name             = cfg.cpu_name,
    param_perf.cpu_load_type        = cfg.cpu_load_type,
    param_perf.cpu_threshold        = cfg.cpu_threshold,
    param_perf.current_threshold    = &current_threshold,
    param_perf.interface_name       = cfg.interface_name;

    pthread_attr_init(&attr_perf);
    pthread_attr_setscope(&attr_perf, PTHREAD_SCOPE_SYSTEM);

    if (cfg.is_joinable) {
        detachstate = PTHREAD_CREATE_JOINABLE;
    } else {
        detachstate = PTHREAD_CREATE_DETACHED;
    }

    pthread_attr_setdetachstate(&attr_perf, detachstate);
    create_failed = pthread_create(&prf_thread_ext, &attr_perf, prf_perf_collect, &param_perf);

    if (create_failed) {
        fprintf(stderr, "** ERROR - performance thread creation failed\n");
        pthread_cancel(prf_thread_ext);
    } else {
#ifdef _GNU_SOURCE
        setname_failed = pthread_setname_np(prf_thread_ext, cfg.thread_name);
#else
        setname_failed = false;
#endif
        if (setname_failed) {
            pthread_cancel(prf_thread_ext);
            fprintf(stderr, "** ERROR - performance thread could not be renamed to '%s'\n", cfg.thread_name);
        } else {
            if (cfg.is_joinable) {
                // PTHREAD_CREATE_JOINABLE: the calling thread waits for until the thread finishes
                join_failed = pthread_join(prf_thread_ext, NULL);
                if (join_failed) {
                    pthread_cancel(prf_thread_ext);
                    fprintf(stderr, "** ERROR - performance thread could not be started\n");
                } else {
                    status = true;
                }
            } else {
                // PTHREAD_CREATE_DETACHED: the thread runs in parallel with the calling thread
                struct timespec delay_req = {0, delay_ms * 1000000L};
                struct timespec wait_act;

                // delay, so that initial reads are completed
                nanosleep(&delay_req, &wait_act);

                while (is_running) {
                    nanosleep(&sleep_req, &wait_act);
                    printf("== detached: %4.2f | is overloaded? %s\n",
                            current_threshold, (current_threshold >= cfg.cpu_threshold) ? PRF_TRUE : PRF_FALSE);
                }

                status = true;
            }
        }
    }

    // ATTENTION: if execution is reached here, it means that the performance thread is stopped.
    pthread_attr_destroy(&attr_perf);

    if (status) {
        printf("\nINFO: application successfully terminated\n");
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "\n** ERROR - abnormal application termination\n");
        return EXIT_FAILURE;
    }
}

static void prf_signal_handler(int signal) {
    // only signal-safe functions are allowed
    // fall-trough, no specific actions
    switch (signal) {
        case SIGINT:  /*  CTRL + C  */
        case SIGQUIT: /*  CTRL + \  */
        case SIGTSTP: /*  CTRL + Z  */
        case SIGTERM: /*  kill      */
        default:
            is_running = false;
            pthread_cancel(prf_thread_ext);
            break;
    }
}

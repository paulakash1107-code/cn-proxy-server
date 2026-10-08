#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include "logger.h"
#include "stats.h"

#define LOG_DIR  "logs"
#define LOG_PATH "logs/proxy.log"

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *log_file = NULL;
static int log_tried = 0;

/* Called with log_lock held. Opens the log file once; if it fails we keep logging to the terminal. */
static void open_log_once(void) {
    if (log_tried) return;
    log_tried = 1;
    mkdir(LOG_DIR, 0755);                 /* ok if it already exists */
    log_file = fopen(LOG_PATH, "a");      /* append: keeps old runs */
    if (!log_file) perror("logger: cannot open " LOG_PATH);
}

void log_request(const char *client_ip, const char *method, const char *url,
                 int status, size_t bytes, const char *cache_status, double ms) {
    char ts[32];
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);              /* thread-safe version of localtime */
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tmv);

    pthread_mutex_lock(&log_lock);
    open_log_once();

    printf("[%s] %s %s %s %d %zu %s %.1fms\n",
           ts, client_ip, method, url, status, bytes, cache_status, ms);
    fflush(stdout);

    if (log_file) {
        fprintf(log_file, "[%s] %s %s %s %d %zu %s %.1fms\n",
                ts, client_ip, method, url, status, bytes, cache_status, ms);
        fflush(log_file);
    }
    pthread_mutex_unlock(&log_lock);

    stats_record(status, bytes, cache_status);   /* feed the counters (Step 4) */
}

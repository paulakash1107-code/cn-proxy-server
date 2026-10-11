#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "access.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) printf("PASS  %s\n", msg); \
    else { printf("FAIL  %s\n", msg); failures++; } } while (0)

static int allowed = 0;
static pthread_mutex_t cnt = PTHREAD_MUTEX_INITIALIZER;

static void *worker(void *arg) {
    (void)arg;
    for (int i = 0; i < 5; i++)
        if (access_rate_ok("10.0.0.99")) { pthread_mutex_lock(&cnt); allowed++; pthread_mutex_unlock(&cnt); }
    return NULL;
}

int main(void) {
    access_init();

    CHECK(access_rate_ok("1.1.1.1") == 1, "limiter disabled by default: request allowed");

    access_set_rate(1.0, 3);                     /* 1 per second, burst of 3 */
    int ok = 0;
    for (int i = 0; i < 6; i++) ok += access_rate_ok("2.2.2.2");
    CHECK(ok == 3, "burst of 3 allowed, the next 3 rejected");
    CHECK(access_rate_ok("3.3.3.3") == 1, "a different client has its own bucket");
    sleep(2);
    CHECK(access_rate_ok("2.2.2.2") == 1, "tokens refill over time");

    access_set_rate(1.0, 10);                    /* 8 threads x 5 attempts = 40 requests at once */
    pthread_t t[8];
    for (int i = 0; i < 8; i++) pthread_create(&t[i], NULL, worker, NULL);
    for (int i = 0; i < 8; i++) pthread_join(t[i], NULL);
    CHECK(allowed >= 10 && allowed <= 11, "8 threads on one IP: about 10 allowed, never all 40");

    FILE *f = fopen("/tmp/test_proxy.conf", "w");
    fprintf(f, "# test\nblock evil.example\nrate 100 3\n");
    fclose(f);
    CHECK(access_load_config("/tmp/test_proxy.conf") == 2, "config file: 2 settings applied");
    CHECK(access_is_allowed("evil.example") == 0, "config file: blocked host is blocked");
    CHECK(access_is_allowed("good.example") == 1, "config file: other hosts are allowed");
    CHECK(access_load_config("/tmp/does-not-exist.conf") == -1, "missing config file returns -1");

    access_destroy();
    printf("\nfailures: %d\n", failures);
    return failures ? 1 : 0;
}

#include "zyro_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <linux/input.h>

#define ZYRO_DEV_PATH "/dev/input/event0"

/* Module state ------------------------------------------------------------- */
static int           g_fd        = -1;
static int           g_base_x    = 0;
static int           g_base_y    = 0;
static int           g_cur_x     = 0;
static int           g_cur_y     = 0;
static pthread_t     g_thread;
static volatile int  g_running   = 0;
static pthread_mutex_t g_mutex   = PTHREAD_MUTEX_INITIALIZER;

/* Background thread -------------------------------------------------------- */
static void *zyro_thread_func(void *arg)
{
    (void)arg;
    struct input_event ev;
    int raw_x = 0, raw_y = 0;

    while (g_running) {
        ssize_t n = read(g_fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) {
            if (!g_running) break;
            continue;
        }

        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X)
                raw_x = ev.value;
            else if (ev.code == ABS_Y)
                raw_y = ev.value;
        }

        if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            pthread_mutex_lock(&g_mutex);
            g_cur_x = raw_x - g_base_x;
            g_cur_y = raw_y - g_base_y;
            pthread_mutex_unlock(&g_mutex);
        }
    }
    return NULL;
}

/* Helper: open device if not already open ---------------------------------- */
static int ensure_open(void)
{
    if (g_fd >= 0)
        return 0;

    g_fd = open(ZYRO_DEV_PATH, O_RDONLY);
    if (g_fd < 0) {
        printf("zyro_api: open %s failed – %s\n", ZYRO_DEV_PATH, strerror(errno));
        return -1;
    }
    return 0;
}

/* Public API --------------------------------------------------------------- */
int board_sync(void)
{
    if (ensure_open() < 0)
        return -1;

    struct input_event ev;
    int got_x = 0, got_y = 0;
    int raw_x = 0, raw_y = 0;

    /* Read events until we have both X and Y from one SYN_REPORT cycle */
    while (!(got_x && got_y)) {
        ssize_t n = read(g_fd, &ev, sizeof(ev));
        if (n != (ssize_t)sizeof(ev)) {
            printf("zyro_api: read error – %s\n", strerror(errno));
            return -1;
        }
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X) { raw_x = ev.value; got_x = 1; }
            if (ev.code == ABS_Y) { raw_y = ev.value; got_y = 1; }
        }
    }

    pthread_mutex_lock(&g_mutex);
    g_base_x = raw_x;
    g_base_y = raw_y;
    g_cur_x  = 0;
    g_cur_y  = 0;
    pthread_mutex_unlock(&g_mutex);

    printf("zyro_api: board_sync base=(%d, %d)\n", raw_x, raw_y);
    return 0;
}

int init_event_thread(void)
{
    if (g_running) {
        printf("zyro_api: thread already running\n");
        return 0;
    }

    if (ensure_open() < 0)
        return -1;

    g_running = 1;
    if (pthread_create(&g_thread, NULL, zyro_thread_func, NULL) != 0) {
        printf("zyro_api: pthread_create failed – %s\n", strerror(errno));
        g_running = 0;
        return -1;
    }
    printf("zyro_api: event thread started\n");
    return 0;
}

int close_event_thread(void)
{
    if (!g_running) {
        printf("zyro_api: thread not running\n");
        return 0;
    }

    g_running = 0;
    pthread_join(g_thread, NULL);
    printf("zyro_api: event thread stopped\n");

    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
    return 0;
}

int zyro_get_value(int *out_x, int *out_y)
{
    if (!g_running)
        return -1;

    pthread_mutex_lock(&g_mutex);
    if (out_x) *out_x = g_cur_x;
    if (out_y) *out_y = g_cur_y;
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

/*
 * thermal_api.c
 *
 * Thermal sensor API for the AHT20 temperature sensor on the BCM2837 board.
 *
 * AHT20 : owned by the built-in kernel hwmon driver (CONFIG_SENSORS_AHT10=y).
 *   Data is read from sysfs:
 *   /sys/class/hwmon/hwmonX/temp1_input  (millidegrees Celsius)
 *
 * Follows the same background-thread pattern as zyro_api:
 *   thermal_init()         -- find the hwmon sysfs device
 *   thermal_start_thread() -- start background polling thread
 *   thermal_get_value()    -- read latest temperature (thread-safe)
 *   thermal_stop_thread()  -- stop background thread
 */

#include "thermal_api.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>

/* ---- hwmon sysfs paths --------------------------------------------------- */

#define AHT20_HWMON_DIR      "/sys/class/hwmon"
#define AHT20_DEV_NAME       "aht20"   /* kernel driver name for AHT10/AHT20 */

/*
 * The kernel aht10 driver caches measurements for at least 2000 ms.
 * Polling faster returns the same stale value, so match that interval.
 */
#define AHT20_POLL_INTERVAL_MS  2000

/* ---- Module state -------------------------------------------------------- */

static char            g_hwmon_path[256] = "";  /* set by thermal_init() */
static float           g_cur_temp        = 0.0f;
static pthread_t       g_thread;
static volatile int    g_running         = 0;
static pthread_mutex_t g_mutex           = PTHREAD_MUTEX_INITIALIZER;

/* ---- AHT20 sysfs helpers ------------------------------------------------- */

static int aht20_find_hwmon_device(void)
{
    DIR *dir = opendir(AHT20_HWMON_DIR);
    if (!dir) {
        printf("thermal_api: cannot open %s \u2013 %s (%d)\n",
               AHT20_HWMON_DIR, strerror(errno), __LINE__);
        return -1;
    }

    int found = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "hwmon", 5) != 0)
            continue;

        char name_path[512];
        snprintf(name_path, sizeof(name_path), "%s/%s/name",
                 AHT20_HWMON_DIR, entry->d_name);

        FILE *f = fopen(name_path, "r");
        if (!f)
            continue;

        char dev_name[64] = "";
        fscanf(f, "%63s", dev_name);
        fclose(f);

        if (strcmp(dev_name, AHT20_DEV_NAME) == 0) {
            snprintf(g_hwmon_path, sizeof(g_hwmon_path),
                     "%s/%s", AHT20_HWMON_DIR, entry->d_name);
            found = 1;
            break;
        }
    }
    closedir(dir);

    if (!found) {
        printf("thermal_api: AHT20 hwmon device not found under %s\n",
               AHT20_HWMON_DIR);
        return -1;
    }

    printf("thermal_api: AHT20 hwmon device at %s\n", g_hwmon_path);
    return 0;
}

static int aht20_sysfs_read(float *temp_c)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/temp1_input", g_hwmon_path);

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("thermal_api: cannot read %s \u2013 %s (%d)\n",
               path, strerror(errno), __LINE__);
        return -1;
    }

    long mdeg = 0;
    int ret = fscanf(f, "%ld", &mdeg);
    fclose(f);

    if (ret != 1) {
        printf("thermal_api: failed to parse AHT20 temperature value\n");
        return -1;
    }

    *temp_c = (float)mdeg / 1000.0f;   /* millidegrees -> degrees Celsius */
    return 0;
}

/* ---- Background thread --------------------------------------------------- */

static void *thermal_thread_func(void *arg)
{
    (void)arg;
    float temp;

    while (g_running) {
        if (aht20_sysfs_read(&temp) == 0) {
            pthread_mutex_lock(&g_mutex);
            g_cur_temp = temp;
            pthread_mutex_unlock(&g_mutex);
        }
        /* Sleep in 100 ms increments so the thread responds to stop quickly */
        int ms = AHT20_POLL_INTERVAL_MS;
        while (ms > 0 && g_running) {
            usleep(100 * 1000);
            ms -= 100;
        }
    }
    return NULL;
}

/* ---- Public API ---------------------------------------------------------- */

int thermal_init(void)
{
    return aht20_find_hwmon_device();
}

int thermal_start_thread(void)
{
    if (g_running) {
        printf("thermal_api: thread already running\n");
        return 0;
    }

    if (g_hwmon_path[0] == '\0') {
        printf("thermal_api: not initialised \u2013 call thermal_init() first\n");
        return -1;
    }

    /* Take one reading immediately so g_cur_temp is valid before first poll */
    aht20_sysfs_read(&g_cur_temp);

    g_running = 1;
    if (pthread_create(&g_thread, NULL, thermal_thread_func, NULL) != 0) {
        printf("thermal_api: pthread_create failed \u2013 %s\n", strerror(errno));
        g_running = 0;
        return -1;
    }
    printf("thermal_api: polling thread started (interval %d ms)\n",
           AHT20_POLL_INTERVAL_MS);
    return 0;
}

int thermal_get_value(float *temp_c)
{
    if (!g_running)
        return -1;

    pthread_mutex_lock(&g_mutex);
    if (temp_c) *temp_c = g_cur_temp;
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

int thermal_stop_thread(void)
{
    if (!g_running) {
        printf("thermal_api: thread not running\n");
        return 0;
    }

    g_running = 0;
    pthread_join(g_thread, NULL);
    printf("thermal_api: polling thread stopped\n");
    return 0;
}

int thermal_close(void)
{
    thermal_stop_thread();
    g_hwmon_path[0] = '\0';
    printf("thermal_api: AHT20 hwmon released\n");
    return 0;
}
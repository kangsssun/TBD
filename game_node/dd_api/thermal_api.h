#ifndef THERMAL_API_H
#define THERMAL_API_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * thermal_api.h
 *
 * Public API for real-time temperature readings from the AHT20 sensor
 * on the BCM2837 board via the kernel hwmon driver (CONFIG_SENSORS_AHT10=y).
 *
 * Follows the same background-thread pattern as zyro_api:
 *
 * Typical usage:
 *   thermal_init();                    // find hwmon sysfs device
 *   thermal_start_thread();            // start background polling
 *
 *   float temp;
 *   thermal_get_value(&temp);          // get latest reading (non-blocking)
 *   printf("%.2f C\n", temp);
 *
 *   thermal_stop_thread();             // stop polling
 *   thermal_close();                   // release resources
 */

/*
 * thermal_init - Locate the AHT20 hwmon sysfs device.
 *
 * Must be called before thermal_start_thread().
 *
 * Returns 0 on success, -1 on error.
 */
int thermal_init(void);

/*
 * thermal_start_thread - Start the background temperature polling thread.
 *
 * The thread reads temp1_input every 2000 ms (the minimum interval
 * at which the kernel aht10 driver refreshes its measurement).
 * Takes one immediate reading so thermal_get_value() is valid right away.
 *
 * Returns 0 on success, -1 on error.
 */
int thermal_start_thread(void);

/*
 * thermal_get_value - Retrieve the latest temperature reading.
 *
 * Non-blocking and thread-safe.  Returns the value captured by the
 * most recent background poll.
 *
 * temp_c : receives temperature in degrees Celsius
 *
 * Returns 0 on success, -1 if the thread is not running.
 */
int thermal_get_value(float *temp_c);

/*
 * thermal_stop_thread - Stop the background polling thread.
 *
 * Returns 0 on success, -1 on error.
 */
int thermal_stop_thread(void);

/*
 * thermal_close - Stop the thread (if running) and release all resources.
 *
 * Returns 0 on success, -1 on error.
 */
int thermal_close(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMAL_API_H */

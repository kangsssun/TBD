#ifndef ZYRO_API_H
#define ZYRO_API_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * zyro_api.h
 *
 * Public API for the zyro (accelerometer) sensor.
 *
 * Typical usage:
 *   board_sync();                       // calibrate base values
 *   init_event_thread();                // start background reading
 *   int x, y; zyro_get_value(&x, &y);  // poll relative values
 *   close_event_thread();               // stop background reading
 */

/*
 * board_sync - Calibrate the sensor by reading the current zyro values
 *              and storing them as the base (zero) reference.
 *
 * After calling this function, subsequent zyro values will be reported
 * relative to the values captured at calibration time.
 *
 * Returns 0 on success, -1 on error.
 */
int board_sync(void);

/*
 * init_event_thread - Start a background thread that continuously reads
 *                     zyro sensor values and makes them available via
 *                     zyro_get_value().
 *
 * Returns 0 on success, -1 on error.
 */
int init_event_thread(void);

/*
 * close_event_thread - Stop the background zyro reading thread.
 *
 * Returns 0 on success, -1 on error.
 */
int close_event_thread(void);

/*
 * zyro_get_value - Retrieve the latest zyro sensor value relative to
 *                  the base established by board_sync().
 *
 * out_x : pointer to receive relative X value
 * out_y : pointer to receive relative Y value
 *
 * Returns 0 on success, -1 if the thread is not running.
 */
int zyro_get_value(int *out_x, int *out_y);

#ifdef __cplusplus
}
#endif

#endif /* ZYRO_API_H */

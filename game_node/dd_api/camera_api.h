#ifdef __cplusplus
extern "C" {
#endif

/*
 * camera_api.h
 *
 * Public API for V4L2 camera capture with live framebuffer display.
 *
 * Typical usage:
 *   camera_init(0, 0, 640, 480);
 *   camera_capture("/tmp/shot.yuv");
 *   camera_close();
 */

/*
 * camera_init - Open the camera and start live display on the framebuffer.
 *
 * Opens /dev/video0 (camera) and /dev/fb0 (framebuffer), sets the capture
 * resolution to width x height, and starts a background thread that
 * continuously reads frames and renders them at screen position (x, y).
 *
 * x, y   : top-left pixel position on the screen to draw the video feed
 * width  : capture (and display) width in pixels
 * height : capture (and display) height in pixels
 *
 * Returns 0 on success, -1 on error.
 */
int camera_init(int x, int y, int width, int height);

/*
 * camera_capture - Save the next live frame to a file.
 *
 * Waits for the next complete frame from the background streaming thread,
 * saves the raw YUYV422 data to filepath, and returns.  The frame is also
 * visible on the framebuffer because the streaming thread paints every frame.
 *
 * filepath : destination path for the raw YUYV422 image (overwritten if exists)
 *
 * Returns 0 on success, -1 on error.
 */
int camera_capture(const char *filepath);

/*
 * camera_close - Stop the camera stream and release all resources.
 *
 * Signals the background thread to stop, waits for it to finish, sends
 * VIDIOC_STREAMOFF, frees all V4L2 buffers, and closes both the camera
 * device and the framebuffer.
 *
 * Returns 0 on success, -1 on error.
 */
int camera_close(void);

#ifdef __cplusplus
}
#endif

/*
 * camera_api.c
 *
 * Implementation of the camera API using V4L2 (MMAP streaming) and a
 * Linux framebuffer.  A dedicated POSIX thread continuously dequeues
 * frames, converts YUYV422 -> RGB888, and blits them to the framebuffer.
 * camera_capture() hooks into that thread to save one frame to disk.
 */

#include "camera_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <pthread.h>
#include <jpeglib.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define CAM_DEV  "/dev/video0"
#define FB_DEV   "/dev/fb0"
#define N_BUFS   4
#define CAP_PATH_MAX 512

/* ------------------------------------------------------------------ */
/* Internal state                                                       */
/* ------------------------------------------------------------------ */

struct cam_buffer {
    void   *start;
    size_t  length;
};

static struct {
    /* devices */
    int fd;
    int fd_fb;

    /* framebuffer mapping */
    char   *map_fb;
    int     size_fb;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    /* V4L2 */
    struct cam_buffer  *buffers;
    unsigned int        n_buffers;
    struct v4l2_format  fmt;

    /* display position */
    int x, y;

    /* streaming thread */
    pthread_t       stream_tid;
    volatile int    stop_flag;

    /* capture synchronisation */
    pthread_mutex_t capture_mutex;
    pthread_cond_t  capture_cond;
    int             capture_pending;   /* 1 = request outstanding */
    char            capture_path[CAP_PATH_MAX];
    int             capture_result;    /* 0 = ok, -1 = error */

    /* saved background region (restored on close) */
    void   *restore_buf;
    size_t  restore_size;
} cam;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static int xioctl(int fh, int request, void *arg)
{
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

/* Clamp an integer to [0, 255]. */
static inline int clamp255(int v)
{
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

/* ------------------------------------------------------------------ */
/* Framebuffer rendering                                                */
/* ------------------------------------------------------------------ */

/*
 * save_region - copy the current framebuffer content under the camera area
 *               into cam.restore_buf so it can be restored later.
 */
static void save_region(void)
{
    int i;
    int width  = (int)cam.fmt.fmt.pix.width;
    int height = (int)cam.fmt.fmt.pix.height;
    int stride = (int)cam.finfo.line_length;
    int bpp    = (int)(cam.vinfo.bits_per_pixel / 8);
    int row_bytes = width * bpp;

    cam.restore_size = (size_t)(height * row_bytes);
    cam.restore_buf  = malloc(cam.restore_size);
    if (!cam.restore_buf) {
        perror("save_region: malloc");
        return;
    }

    for (i = 0; i < height; i++) {
        int fb_offset  = (i + cam.y) * stride + cam.x * bpp;
        int buf_offset = i * row_bytes;
        memcpy((char *)cam.restore_buf + buf_offset,
               cam.map_fb + fb_offset,
               (size_t)row_bytes);
    }
}

/*
 * restore_region - write cam.restore_buf back to the framebuffer, undoing
 *                  all pixels the camera API painted.
 */
static void restore_region(void)
{
    int i;
    int width  = (int)cam.fmt.fmt.pix.width;
    int height = (int)cam.fmt.fmt.pix.height;
    int stride = (int)cam.finfo.line_length;
    int bpp    = (int)(cam.vinfo.bits_per_pixel / 8);
    int row_bytes = width * bpp;

    if (!cam.restore_buf)
        return;

    for (i = 0; i < height; i++) {
        int fb_offset  = (i + cam.y) * stride + cam.x * bpp;
        int buf_offset = i * row_bytes;
        memcpy(cam.map_fb + fb_offset,
               (const char *)cam.restore_buf + buf_offset,
               (size_t)row_bytes);
    }

    free(cam.restore_buf);
    cam.restore_buf = NULL;
}

/*
 * display_frame - paint one YUYV422 frame onto the framebuffer at (cam.x, cam.y).
 *
 * Converts every pair of YUYV422 pixels to RGB888 and writes them into the
 * memory-mapped framebuffer.  Assumes a 32-bpp framebuffer.
 */
static void display_frame(const void *p)
{
    int i, j;
    const unsigned char *pp = (const unsigned char *)p;
    int width  = (int)cam.fmt.fmt.pix.width;
    int height = (int)cam.fmt.fmt.pix.height;
    int stride = (int)cam.finfo.line_length;  /* bytes per screen row */
    int bpp    = (int)(cam.vinfo.bits_per_pixel / 8);

    for (i = 0; i < height; i++) {
        for (j = 0; j < width / 2; j++) {
            /* Each group of 4 bytes: Y0 U Y1 V (YUYV packed) */
            const unsigned char *src = pp + (i * width * 2) + (j * 4);
            unsigned char Y0 = src[0];
            unsigned char U  = src[1];
            unsigned char Y1 = src[2];
            unsigned char V  = src[3];

            int R, G, B;
            unsigned int pixel;
            int screen_offset;

            /* --- first pixel (Y0) --- */
            R = clamp255((int)(Y0 + 1.4075 * (V - 128)));
            G = clamp255((int)(Y0 - 0.3455 * (U - 128) - 0.7169 * (V - 128)));
            B = clamp255((int)(Y0 + 1.7790 * (U - 128)));
            pixel = ((unsigned int)R << 16) | ((unsigned int)G << 8) | (unsigned int)B;

            screen_offset = (i + cam.y) * stride + (j * 2 + cam.x) * bpp;
            *(unsigned int *)(cam.map_fb + screen_offset) = pixel;

            /* --- second pixel (Y1) --- */
            R = clamp255((int)(Y1 + 1.4075 * (V - 128)));
            G = clamp255((int)(Y1 - 0.3455 * (U - 128) - 0.7169 * (V - 128)));
            B = clamp255((int)(Y1 + 1.7790 * (U - 128)));
            pixel = ((unsigned int)R << 16) | ((unsigned int)G << 8) | (unsigned int)B;

            screen_offset = (i + cam.y) * stride + (j * 2 + 1 + cam.x) * bpp;
            *(unsigned int *)(cam.map_fb + screen_offset) = pixel;
        }
    }
}

/* ------------------------------------------------------------------ */
/* File save                                                            */
/* ------------------------------------------------------------------ */

/*
 * save_frame_jpeg - convert YUYV422 to RGB24 and write a JPEG file.
 * quality is fixed at 90.  Returns 0 on success, -1 on error.
 */
static int save_frame_jpeg(const void *p, int width, int height, const char *path)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr       jerr;
    FILE         *fp;
    JSAMPROW      row[1];
    unsigned char *rgb;
    int i, j;
    const unsigned char *pp = (const unsigned char *)p;

    /* ---- YUYV422 -> packed RGB24 ---- */
    rgb = malloc((size_t)(width * height * 3));
    if (!rgb) {
        perror("save_frame_jpeg: malloc");
        return -1;
    }

    for (i = 0; i < height; i++) {
        for (j = 0; j < width / 2; j++) {
            const unsigned char *src = pp + (i * width * 2) + (j * 4);
            unsigned char *dst = rgb + (i * width + j * 2) * 3;
            unsigned char Y0 = src[0], U = src[1], Y1 = src[2], V = src[3];

            /* first pixel */
            dst[0] = (unsigned char)clamp255((int)(Y0 + 1.4075  * (V - 128)));
            dst[1] = (unsigned char)clamp255((int)(Y0 - 0.3455  * (U - 128) - 0.7169 * (V - 128)));
            dst[2] = (unsigned char)clamp255((int)(Y0 + 1.7790  * (U - 128)));
            /* second pixel */
            dst[3] = (unsigned char)clamp255((int)(Y1 + 1.4075  * (V - 128)));
            dst[4] = (unsigned char)clamp255((int)(Y1 - 0.3455  * (U - 128) - 0.7169 * (V - 128)));
            dst[5] = (unsigned char)clamp255((int)(Y1 + 1.7790  * (U - 128)));
        }
    }

    /* ---- compress to JPEG ---- */
    fp = fopen(path, "wb");
    if (!fp) {
        perror("save_frame_jpeg: fopen");
        free(rgb);
        return -1;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width      = (JDIMENSION)width;
    cinfo.image_height     = (JDIMENSION)height;
    cinfo.input_components = 3;
    cinfo.in_color_space   = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        row[0] = rgb + cinfo.next_scanline * (JDIMENSION)width * 3;
        jpeg_write_scanlines(&cinfo, row, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(fp);
    free(rgb);

    return 0;
}

/* ------------------------------------------------------------------ */
/* Streaming thread                                                     */
/* ------------------------------------------------------------------ */

/*
 * stream_thread - runs continuously while the camera is active.
 *
 * Each iteration:
 *   1. select() waits for the camera to have a ready frame (2 s timeout).
 *   2. VIDIOC_DQBUF dequeues the filled buffer.
 *   3. display_frame() blits YUYV->RGB to the framebuffer.
 *   4. If camera_capture() has posted a request, save the frame to disk
 *      and signal completion.
 *   5. VIDIOC_QBUF re-queues the buffer for the next capture.
 */
static void *stream_thread(void *arg)
{
    (void)arg;

    while (!cam.stop_flag) {
        fd_set fds;
        struct timeval tv;
        struct v4l2_buffer buf;
        int r;

        FD_ZERO(&fds);
        FD_SET(cam.fd, &fds);
        tv.tv_sec  = 2;
        tv.tv_usec = 0;

        r = select(cam.fd + 1, &fds, NULL, NULL, &tv);
        if (r == -1) {
            if (errno == EINTR)
                continue;
            perror("stream_thread: select");
            break;
        }
        if (r == 0)
            continue;   /* timeout – retry */

        CLEAR(buf);
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (xioctl(cam.fd, VIDIOC_DQBUF, &buf) == -1) {
            if (errno == EAGAIN)
                continue;
            perror("stream_thread: VIDIOC_DQBUF");
            break;
        }

        /* paint to screen */
        display_frame(cam.buffers[buf.index].start);

        /* fulfill a pending camera_capture() request */
        pthread_mutex_lock(&cam.capture_mutex);
        if (cam.capture_pending) {
            cam.capture_result = save_frame_jpeg(
                                    cam.buffers[buf.index].start,
                                    (int)cam.fmt.fmt.pix.width,
                                    (int)cam.fmt.fmt.pix.height,
                                    cam.capture_path);
            cam.capture_pending = 0;
            pthread_cond_signal(&cam.capture_cond);
        }
        pthread_mutex_unlock(&cam.capture_mutex);

        /* return buffer to the driver */
        if (xioctl(cam.fd, VIDIOC_QBUF, &buf) == -1)
            perror("stream_thread: VIDIOC_QBUF");
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int camera_init(int x, int y, int width, int height)
{
    struct v4l2_capability     cap;
    struct v4l2_requestbuffers req;
    enum   v4l2_buf_type       type;
    unsigned int i;

    memset(&cam, 0, sizeof(cam));
    cam.fd    = -1;
    cam.fd_fb = -1;
    cam.x     = x;
    cam.y     = y;

    pthread_mutex_init(&cam.capture_mutex, NULL);
    pthread_cond_init(&cam.capture_cond, NULL);

    /* ---- open camera device ---- */
    cam.fd = open(CAM_DEV, O_RDWR | O_NONBLOCK, 0);
    if (cam.fd == -1) {
        perror("camera_init: open " CAM_DEV);
        goto err_mutex;
    }

    if (xioctl(cam.fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("camera_init: VIDIOC_QUERYCAP");
        goto err_cam;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "camera_init: %s is not a capture device\n", CAM_DEV);
        goto err_cam;
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "camera_init: %s does not support streaming\n", CAM_DEV);
        goto err_cam;
    }

    /* ---- set capture format ---- */
    CLEAR(cam.fmt);
    cam.fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam.fmt.fmt.pix.width       = (unsigned int)width;
    cam.fmt.fmt.pix.height      = (unsigned int)height;
    cam.fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    cam.fmt.fmt.pix.field       = V4L2_FIELD_NONE;
    cam.fmt.fmt.pix.sizeimage   = (unsigned int)(width * height * 2);

    if (xioctl(cam.fd, VIDIOC_S_FMT, &cam.fmt) == -1) {
        perror("camera_init: VIDIOC_S_FMT");
        goto err_cam;
    }

    /* ---- request MMAP buffers ---- */
    CLEAR(req);
    req.count  = N_BUFS;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(cam.fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("camera_init: VIDIOC_REQBUFS");
        goto err_cam;
    }
    if (req.count < 2) {
        fprintf(stderr, "camera_init: insufficient buffer memory\n");
        goto err_cam;
    }

    cam.buffers = calloc(req.count, sizeof(*cam.buffers));
    if (!cam.buffers) {
        perror("camera_init: calloc");
        goto err_cam;
    }

    for (cam.n_buffers = 0; cam.n_buffers < req.count; cam.n_buffers++) {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = cam.n_buffers;

        if (xioctl(cam.fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("camera_init: VIDIOC_QUERYBUF");
            goto err_bufs;
        }

        cam.buffers[cam.n_buffers].length = buf.length;
        cam.buffers[cam.n_buffers].start  =
            mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                 MAP_SHARED, cam.fd, buf.m.offset);

        if (cam.buffers[cam.n_buffers].start == MAP_FAILED) {
            perror("camera_init: mmap buffer");
            goto err_bufs;
        }
    }

    /* ---- open framebuffer ---- */
    cam.fd_fb = open(FB_DEV, O_RDWR);
    if (cam.fd_fb == -1) {
        perror("camera_init: open " FB_DEV);
        goto err_bufs;
    }

    if (ioctl(cam.fd_fb, FBIOGET_VSCREENINFO, &cam.vinfo) == -1) {
        perror("camera_init: FBIOGET_VSCREENINFO");
        goto err_fb;
    }
    if (ioctl(cam.fd_fb, FBIOGET_FSCREENINFO, &cam.finfo) == -1) {
        perror("camera_init: FBIOGET_FSCREENINFO");
        goto err_fb;
    }
    if (cam.vinfo.bits_per_pixel != 32) {
        fprintf(stderr, "camera_init: framebuffer must be 32 bpp (got %u)\n",
                cam.vinfo.bits_per_pixel);
        goto err_fb;
    }

    cam.size_fb = (int)(cam.vinfo.xres * cam.vinfo.yres *
                        cam.vinfo.bits_per_pixel / 8);
    cam.map_fb  = mmap(0, (size_t)cam.size_fb,
                       PROT_READ | PROT_WRITE, MAP_SHARED, cam.fd_fb, 0);
    if (cam.map_fb == MAP_FAILED) {
        perror("camera_init: mmap framebuffer");
        goto err_fb;
    }

    /* save the screen content under the camera area so close() can restore it */
    save_region();

    /* ---- queue all buffers ---- */
    for (i = 0; i < cam.n_buffers; i++) {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (xioctl(cam.fd, VIDIOC_QBUF, &buf) == -1) {
            perror("camera_init: VIDIOC_QBUF");
            goto err_fb_map;
        }
    }

    /* ---- start streaming ---- */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(cam.fd, VIDIOC_STREAMON, &type) == -1) {
        perror("camera_init: VIDIOC_STREAMON");
        goto err_fb_map;
    }

    /* ---- launch display thread ---- */
    cam.stop_flag = 0;
    if (pthread_create(&cam.stream_tid, NULL, stream_thread, NULL) != 0) {
        perror("camera_init: pthread_create");
        goto err_streamon;
    }

    return 0;

    /* ---- error unwind ---- */
err_streamon:
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(cam.fd, VIDIOC_STREAMOFF, &type);
err_fb_map:
    munmap(cam.map_fb, (size_t)cam.size_fb);
err_fb:
    close(cam.fd_fb);
err_bufs:
    for (i = 0; i < cam.n_buffers; i++)
        if (cam.buffers[i].start && cam.buffers[i].start != MAP_FAILED)
            munmap(cam.buffers[i].start, cam.buffers[i].length);
    free(cam.buffers);
err_cam:
    close(cam.fd);
err_mutex:
    pthread_mutex_destroy(&cam.capture_mutex);
    pthread_cond_destroy(&cam.capture_cond);
    return -1;
}

int camera_capture(const char *filepath)
{
    if (!filepath || filepath[0] == '\0') {
        fprintf(stderr, "camera_capture: filepath must not be empty\n");
        return -1;
    }

    pthread_mutex_lock(&cam.capture_mutex);

    strncpy(cam.capture_path, filepath, CAP_PATH_MAX - 1);
    cam.capture_path[CAP_PATH_MAX - 1] = '\0';
    cam.capture_pending = 1;

    /* block until the streaming thread saves the next frame */
    while (cam.capture_pending)
        pthread_cond_wait(&cam.capture_cond, &cam.capture_mutex);

    int result = cam.capture_result;
    pthread_mutex_unlock(&cam.capture_mutex);

    return result;
}

int camera_close(void)
{
    enum v4l2_buf_type type;
    unsigned int i;

    /* tell the streaming thread to exit and wait for it */
    cam.stop_flag = 1;
    pthread_join(cam.stream_tid, NULL);

    /* stop the V4L2 stream */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(cam.fd, VIDIOC_STREAMOFF, &type) == -1)
        perror("camera_close: VIDIOC_STREAMOFF");

    /* unmap and free capture buffers */
    for (i = 0; i < cam.n_buffers; i++)
        munmap(cam.buffers[i].start, cam.buffers[i].length);
    free(cam.buffers);

    /* close camera */
    close(cam.fd);
    cam.fd = -1;

    /* restore the screen to what it looked like before camera_init() */
    restore_region();

    /* unmap and close framebuffer */
    munmap(cam.map_fb, (size_t)cam.size_fb);
    close(cam.fd_fb);
    cam.fd_fb = -1;

    pthread_mutex_destroy(&cam.capture_mutex);
    pthread_cond_destroy(&cam.capture_cond);

    return 0;
}

#ifndef CAMERA_STREAM_H
#define CAMERA_STREAM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <linux/videodev2.h>
#include <curl/curl.h>
#include <jpeglib.h>

#define CONFIG_FILE     "/etc/camera-stream/stream.conf"
#define LOG_FILE        "/var/log/camera-stream.log"
#define SNAPSHOT_DIR    "/var/lib/image-uploader"

#define DEFAULT_SERVER_STREAM_URL   "http://100.125.136.31/stream/push"
#define DEFAULT_SERVER_SNAPSHOT_URL "http://100.125.136.31/images"
#define DEFAULT_DEVICE_ID           "rpi4"
#define DEFAULT_V4L2_DEV            "/dev/video0"
#define DEFAULT_WIDTH               640
#define DEFAULT_HEIGHT              480
#define DEFAULT_FPS                 30
#define DEFAULT_SNAPSHOT_INTERVAL   30   /* seconds between snapshots */
#define DEFAULT_JPEG_QUALITY        75   /* 1-100; 75 balances quality and size */
#define DEFAULT_API_KEY             ""

#define N_BUFFERS             4
#define OFFLINE_FRAMES_DIR    "/var/spool/camera-stream"
#define MAX_OFFLINE_FRAMES    100
#define OFFLINE_MIN_FREE_BYTES (20ULL * 1024ULL * 1024ULL)

typedef struct {
    void   *start;
    size_t  length;
} MmapBuffer;

typedef struct {
    char server_stream_url  [256];
    char server_snapshot_url[256];
    char device_id          [64];
    char v4l2_dev           [64];
    char api_key            [256];
    int  width;
    int  height;
    int  fps;
    int  snapshot_interval;
    int  jpeg_quality;
} StreamConfig;

/* Latest JPEG frame — shared between capture and push threads */
typedef struct {
    unsigned char  *data;
    size_t          size;
    struct timespec ts;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    int             ready;
} FrameBuffer;

void   load_config(StreamConfig *cfg);
int    open_camera(const StreamConfig *cfg, MmapBuffer **bufs, int *n_bufs);
void   close_camera(int fd, MmapBuffer *bufs, int n_bufs);
size_t yuyv_to_jpeg(const unsigned char *yuyv, int width, int height,
                    int quality, unsigned char **out);
void   log_message(const char *fmt, ...);
void   signal_handler(int signum);

#endif

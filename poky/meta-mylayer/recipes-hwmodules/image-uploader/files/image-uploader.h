#ifndef IMAGE_UPLOADER_H
#define IMAGE_UPLOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <curl/curl.h>

/* ------------------------------------------------------------------ */
/*  Compile-time configuration                                          */
/* ------------------------------------------------------------------ */
#define CONFIG_FILE         "/etc/image-uploader/uploader.conf"
#define LOG_FILE            "/var/log/image-uploader.log"

#define DEFAULT_SERVER_URL  "http://10.10.23.10/images"
#define DEFAULT_DEVICE_ID   "rpi4"
#define DEFAULT_V4L2_DEV    "/dev/video0"
#define DEFAULT_CAPTURE_DIR "/var/lib/image-uploader"
#define DEFAULT_WIDTH       1280
#define DEFAULT_HEIGHT      720
#define DEFAULT_INTERVAL    30          /* seconds between captures     */
#define DEFAULT_MAX_RETRIES 3
#define DEFAULT_RETRY_DELAY 5           /* seconds between retries      */

/* ------------------------------------------------------------------ */
/*  Runtime configuration (loaded from file)                           */
/* ------------------------------------------------------------------ */
typedef struct {
    char server_url [256];
    char device_id  [64];
    char v4l2_dev   [64];
    char capture_dir[256];
    int  width;
    int  height;
    int  interval;
    int  max_retries;
    int  retry_delay;
} UploaderConfig;

/* ------------------------------------------------------------------ */
/*  Function declarations                                              */
/* ------------------------------------------------------------------ */
void load_config(UploaderConfig *cfg);
int  capture_image(const UploaderConfig *cfg, char *out_path, size_t out_len);
int  upload_image (const UploaderConfig *cfg, const char *filepath);
void log_message  (const char *fmt, ...);
void signal_handler(int signum);
void cleanup(void);

#endif /* IMAGE_UPLOADER_H */
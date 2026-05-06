#include "image-uploader.h"
#include <fcntl.h>
#include <sys/file.h>

#define CAMERA_LOCK_PATH "/var/lock/camera_v4l2.lock"

/* ------------------------------------------------------------------ */
/*  Globals                                                             */
/* ------------------------------------------------------------------ */
static volatile int running      = 1;
static volatile int reload_flag  = 0;
static FILE        *log_fp       = NULL;

/* ------------------------------------------------------------------ */
/*  Logging                                                             */
/* ------------------------------------------------------------------ */
void log_message(const char *fmt, ...)
{
    va_list args;

    if (!log_fp)
        log_fp = fopen(LOG_FILE, "a");

    va_start(args, fmt);
    if (log_fp) {
        /* Prefix with timestamp */
        time_t now = time(NULL);
        char tbuf[32];
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(log_fp, "[%s] ", tbuf);
        vfprintf(log_fp, fmt, args);
        fprintf(log_fp, "\n");
        fflush(log_fp);
    } else {
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
    }
    va_end(args);
}

/* ------------------------------------------------------------------ */
/*  Signal handling                                                     */
/* ------------------------------------------------------------------ */
void signal_handler(int signum)
{
    if (signum == SIGHUP) {
        reload_flag = 1;
    } else {
        log_message("Received signal %d, shutting down...", signum);
        running = 0;
    }
}

/* ------------------------------------------------------------------ */
/*  Cleanup                                                             */
/* ------------------------------------------------------------------ */
void cleanup(void)
{
    curl_global_cleanup();
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

/* ------------------------------------------------------------------ */
/*  Configuration loader                                                */
/* ------------------------------------------------------------------ */
void load_config(UploaderConfig *cfg)
{
    /* Defaults */
    strncpy(cfg->server_url,  DEFAULT_SERVER_URL,  sizeof(cfg->server_url)  - 1);
    strncpy(cfg->device_id,   DEFAULT_DEVICE_ID,   sizeof(cfg->device_id)   - 1);
    strncpy(cfg->v4l2_dev,    DEFAULT_V4L2_DEV,    sizeof(cfg->v4l2_dev)    - 1);
    strncpy(cfg->capture_dir, DEFAULT_CAPTURE_DIR, sizeof(cfg->capture_dir) - 1);
    strncpy(cfg->api_key,     DEFAULT_API_KEY,     sizeof(cfg->api_key)     - 1);
    cfg->width       = DEFAULT_WIDTH;
    cfg->height      = DEFAULT_HEIGHT;
    cfg->interval    = DEFAULT_INTERVAL;
    cfg->max_retries = DEFAULT_MAX_RETRIES;
    cfg->retry_delay = DEFAULT_RETRY_DELAY;

    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) {
        log_message("Config file not found (%s), using defaults", CONFIG_FILE);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        line[strcspn(line, "\n")] = '\0';

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\0')
            continue;

        char key[64], val[448];
        if (sscanf(line, "%63[^=]=%447s", key, val) != 2)
            continue;

        if      (strcmp(key, "server_url")  == 0) strncpy(cfg->server_url,  val, sizeof(cfg->server_url)  - 1);
        else if (strcmp(key, "device_id")   == 0) strncpy(cfg->device_id,   val, sizeof(cfg->device_id)   - 1);
        else if (strcmp(key, "v4l2_dev")    == 0) strncpy(cfg->v4l2_dev,    val, sizeof(cfg->v4l2_dev)    - 1);
        else if (strcmp(key, "capture_dir") == 0) strncpy(cfg->capture_dir, val, sizeof(cfg->capture_dir) - 1);
        else if (strcmp(key, "width")       == 0) cfg->width       = atoi(val);
        else if (strcmp(key, "height")      == 0) cfg->height      = atoi(val);
        else if (strcmp(key, "interval")    == 0) cfg->interval    = atoi(val);
        else if (strcmp(key, "max_retries") == 0) cfg->max_retries = atoi(val);
        else if (strcmp(key, "retry_delay") == 0) cfg->retry_delay = atoi(val);
        else if (strcmp(key, "api_key")     == 0) strncpy(cfg->api_key, val, sizeof(cfg->api_key) - 1);
    }

    fclose(f);
    log_message("Config loaded: url=%s dev=%s interval=%ds",
                cfg->server_url, cfg->v4l2_dev, cfg->interval);
}

/* ------------------------------------------------------------------ */
/*  Image capture via v4l2-ctl                                          */
/*                                                                      */
/*  Uses v4l2-ctl (from v4l-utils) — same tool used to test manually.  */
/*  Saves to <capture_dir>/<device_id>_<timestamp>.jpg                  */
/* ------------------------------------------------------------------ */
int capture_image(const UploaderConfig *cfg, char *out_path, size_t out_len)
{
    mkdir(cfg->capture_dir, 0755);

    time_t ts = time(NULL);
    snprintf(out_path, out_len, "%s/%s_%ld.jpg",
             cfg->capture_dir, cfg->device_id, (long)ts);

    /* Blocking lock — snapshot waits for camera-stream to finish its frame */
    int lock_fd = open(CAMERA_LOCK_PATH, O_CREAT | O_RDWR, 0644);
    if (lock_fd < 0) {
        log_message("ERROR: cannot open lock file %s", CAMERA_LOCK_PATH);
        return -1;
    }
    if (flock(lock_fd, LOCK_EX) < 0) {
        log_message("ERROR: flock failed");
        close(lock_fd);
        return -1;
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "v4l2-ctl --device=%s "
             "--set-fmt-video=width=%d,height=%d,pixelformat=JPEG "
             "--stream-mmap "
             "--stream-to=%s "
             "--stream-count=1 "
             ">> /var/log/image-uploader.log 2>&1",
             cfg->v4l2_dev, cfg->width, cfg->height, out_path);

    log_message("Capturing: %s", cmd);
    int ret = system(cmd);

    flock(lock_fd, LOCK_UN);
    close(lock_fd);

    if (ret != 0) {
        log_message("ERROR: v4l2-ctl failed (exit %d)", ret);
        return -1;
    }

    struct stat st;
    if (stat(out_path, &st) < 0 || st.st_size == 0) {
        log_message("ERROR: captured file missing or empty: %s", out_path);
        return -1;
    }

    log_message("Captured %s (%ld bytes)", out_path, (long)st.st_size);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  libcurl response sink (discard body)                                */
/* ------------------------------------------------------------------ */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    (void)contents; (void)userp;
    return size * nmemb;
}

/* ------------------------------------------------------------------ */
/*  Upload image via multipart/form-data POST                           */
/*                                                                      */
/*  Mirrors the manual curl command:                                    */
/*    curl -X POST <server_url>                                         */
/*         -F "image=@<filepath>"                                       */
/*         -F "device_id=<device_id>"                                   */
/*         -F "timestamp=<unix_ts>"                                     */
/* ------------------------------------------------------------------ */
int upload_image(const UploaderConfig *cfg, const char *filepath)
{
    CURL     *curl;
    CURLcode  res;
    int       success = 0;

    curl = curl_easy_init();
    if (!curl) {
        log_message("ERROR: curl_easy_init() failed");
        return -1;
    }

    /* Build timestamp string */
    char ts_str[32];
    snprintf(ts_str, sizeof(ts_str), "%ld", (long)time(NULL));

    /* Build multipart form */
    curl_mime *form = curl_mime_init(curl);

    /* Field: image file */
    curl_mimepart *part = curl_mime_addpart(form);
    curl_mime_name(part, "image");
    curl_mime_filedata(part, filepath);
    curl_mime_type(part, "image/jpeg");

    /* Field: device_id */
    part = curl_mime_addpart(form);
    curl_mime_name(part, "device_id");
    curl_mime_data(part, cfg->device_id, CURL_ZERO_TERMINATED);

    /* Field: timestamp */
    part = curl_mime_addpart(form);
    curl_mime_name(part, "timestamp");
    curl_mime_data(part, ts_str, CURL_ZERO_TERMINATED);

    /* Add X-API-Key header for server authentication */
    struct curl_slist *headers = NULL;
    char api_key_header[320];
    snprintf(api_key_header, sizeof(api_key_header), "X-API-Key: %s", cfg->api_key);
    headers = curl_slist_append(headers, api_key_header);

    curl_easy_setopt(curl, CURLOPT_URL,           cfg->server_url);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST,      form);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       30L);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        log_message("ERROR: curl_easy_perform() failed: %s",
                    curl_easy_strerror(res));
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code == 200 || http_code == 201) {
            log_message("Upload OK (HTTP %ld): %s", http_code, filepath);
            success = 1;
        } else {
            log_message("ERROR: server returned HTTP %ld for %s",
                        http_code, filepath);
        }
    }

    curl_mime_free(form);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return success ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/*  Main loop                                                           */
/* ------------------------------------------------------------------ */
int main(void)
{
    UploaderConfig cfg;

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP,  signal_handler);

    curl_global_init(CURL_GLOBAL_ALL);

    load_config(&cfg);

    log_message("image-uploader started (interval=%ds, server=%s)",
                cfg.interval, cfg.server_url);

    while (running) {

        /* Hot-reload config on SIGHUP */
        if (reload_flag) {
            load_config(&cfg);
            reload_flag = 0;
        }

        char filepath[512];

        /* 1. Capture */
        if (capture_image(&cfg, filepath, sizeof(filepath)) < 0) {
            log_message("Capture failed, will retry in %ds", cfg.interval);
            sleep(cfg.interval);
            continue;
        }

        /* 2. Upload with retry */
        int uploaded = 0;
        for (int attempt = 1; attempt <= cfg.max_retries && !uploaded; attempt++) {
            log_message("Upload attempt %d/%d for %s",
                        attempt, cfg.max_retries, filepath);
            if (upload_image(&cfg, filepath) == 0) {
                uploaded = 1;
            } else if (attempt < cfg.max_retries) {
                log_message("Retrying in %ds...", cfg.retry_delay);
                sleep(cfg.retry_delay);
            }
        }

        if (!uploaded)
            log_message("WARNING: all upload attempts failed for %s", filepath);

        /* 3. Wait for next cycle */
        for (int i = 0; i < cfg.interval && running; i++)
            sleep(1);
    }

    cleanup();
    log_message("image-uploader stopped");
    return 0;
}
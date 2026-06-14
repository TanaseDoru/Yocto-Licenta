#include "camera-stream.h"

static volatile int running     = 1;
static volatile int reload_flag = 0;
static FILE        *log_fp      = NULL;
static FrameBuffer  frame_buf;

/* ------------------------------------------------------------------ */
/*  Logging                                                             */
/* ------------------------------------------------------------------ */
void log_message(const char *fmt, ...)
{
    va_list args;
    if (!log_fp) log_fp = fopen(LOG_FILE, "a");
    va_start(args, fmt);
    if (log_fp) {
        time_t now = time(NULL);
        char tbuf[32];
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(log_fp, "[%s] ", tbuf);
        vfprintf(log_fp, fmt, args);
        fprintf(log_fp, "\n");
        fflush(log_fp);
    }
    va_end(args);
}

void signal_handler(int signum)
{
    if (signum == SIGHUP) reload_flag = 1;
    else { log_message("Signal %d, shutting down...", signum); running = 0; }
}

/* ------------------------------------------------------------------ */
/*  Configuration                                                       */
/* ------------------------------------------------------------------ */
void load_config(StreamConfig *cfg)
{
    strncpy(cfg->server_stream_url,   DEFAULT_SERVER_STREAM_URL,   sizeof(cfg->server_stream_url)   - 1);
    strncpy(cfg->server_snapshot_url, DEFAULT_SERVER_SNAPSHOT_URL, sizeof(cfg->server_snapshot_url) - 1);
    strncpy(cfg->device_id,  DEFAULT_DEVICE_ID,  sizeof(cfg->device_id)  - 1);
    strncpy(cfg->v4l2_dev,   DEFAULT_V4L2_DEV,   sizeof(cfg->v4l2_dev)   - 1);
    strncpy(cfg->api_key,    DEFAULT_API_KEY,     sizeof(cfg->api_key)    - 1);
    cfg->width             = DEFAULT_WIDTH;
    cfg->height            = DEFAULT_HEIGHT;
    cfg->fps               = DEFAULT_FPS;
    cfg->snapshot_interval = DEFAULT_SNAPSHOT_INTERVAL;
    cfg->jpeg_quality      = DEFAULT_JPEG_QUALITY;

    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) { log_message("Config not found, using defaults"); return; }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        char key[64], val[448];
        if (sscanf(line, "%63[^=]=%447s", key, val) != 2) continue;
        if      (!strcmp(key, "server_stream_url"))   strncpy(cfg->server_stream_url,   val, sizeof(cfg->server_stream_url)   - 1);
        else if (!strcmp(key, "server_snapshot_url")) strncpy(cfg->server_snapshot_url, val, sizeof(cfg->server_snapshot_url) - 1);
        else if (!strcmp(key, "device_id"))           strncpy(cfg->device_id,  val, sizeof(cfg->device_id)  - 1);
        else if (!strcmp(key, "v4l2_dev"))            strncpy(cfg->v4l2_dev,   val, sizeof(cfg->v4l2_dev)   - 1);
        else if (!strcmp(key, "api_key"))             strncpy(cfg->api_key,    val, sizeof(cfg->api_key)    - 1);
        else if (!strcmp(key, "width"))               cfg->width             = atoi(val);
        else if (!strcmp(key, "height"))              cfg->height            = atoi(val);
        else if (!strcmp(key, "fps"))                 cfg->fps               = atoi(val);
        else if (!strcmp(key, "snapshot_interval"))   cfg->snapshot_interval = atoi(val);
        else if (!strcmp(key, "jpeg_quality"))        cfg->jpeg_quality      = atoi(val);
    }
    fclose(f);
    log_message("Config: url=%s res=%dx%d fps=%d snap=%ds quality=%d",
                cfg->server_stream_url, cfg->width, cfg->height,
                cfg->fps, cfg->snapshot_interval, cfg->jpeg_quality);
}

/* ------------------------------------------------------------------ */
/*  YUYV → JPEG conversion (CPU-side, no GPU involved)                 */
/* ------------------------------------------------------------------ */

/*
 * Converts a YUYV (YUV 4:2:2 packed) frame to a JPEG buffer.
 * The caller must free(*out) when done.
 * Returns the JPEG size in bytes, or 0 on failure.
 *
 * Why CPU JPEG instead of asking the GPU (MJPEG format):
 *   The bcm2835_v4l2 driver routes MJPEG encoding through MMAL/VCHIQ
 *   (the CPU↔GPU mailbox). At 30fps, this saturates the VCHIQ channel,
 *   causing firmware transaction timeouts that kill the camera pipeline.
 *   YUYV is a raw passthrough from the sensor — the GPU does no encoding —
 *   and libjpeg-turbo compresses 640×480 in ~3ms on Cortex-A72.
 */
size_t yuyv_to_jpeg(const unsigned char *yuyv, int width, int height,
                    int quality, unsigned char **out)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr       jerr;

    *out = NULL;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    unsigned long outsize = 0;
    jpeg_mem_dest(&cinfo, out, &outsize);

    cinfo.image_width      = (JDIMENSION)width;
    cinfo.image_height     = (JDIMENSION)height;
    cinfo.input_components = 3;
    cinfo.in_color_space   = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    unsigned char *row = malloc((size_t)width * 3);
    if (!row) {
        jpeg_destroy_compress(&cinfo);
        free(*out);
        *out = NULL;
        return 0;
    }

    /* YUYV packs 2 pixels per 4 bytes: Y0 U Y1 V */
    while (cinfo.next_scanline < cinfo.image_height) {
        const unsigned char *src = yuyv + (size_t)cinfo.next_scanline * (size_t)width * 2;
        unsigned char *dst = row;

        for (int x = 0; x < width; x += 2) {
            int y0 = src[0];
            int u  = (int)src[1] - 128;
            int y1 = src[2];
            int v  = (int)src[3] - 128;
            src += 4;

            /* BT.601 YUV→RGB (integer approximation, no FPU pressure) */
            int r, g, b;

            r = y0 + (359 * v) / 256;
            g = y0 - (88  * u) / 256 - (183 * v) / 256;
            b = y0 + (454 * u) / 256;
            dst[0] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
            dst[1] = (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g);
            dst[2] = (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b);
            dst += 3;

            r = y1 + (359 * v) / 256;
            g = y1 - (88  * u) / 256 - (183 * v) / 256;
            b = y1 + (454 * u) / 256;
            dst[0] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
            dst[1] = (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g);
            dst[2] = (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b);
            dst += 3;
        }

        JSAMPROW ptr = row;
        jpeg_write_scanlines(&cinfo, &ptr, 1);
    }

    free(row);
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return (size_t)outsize;
}

/* ------------------------------------------------------------------ */
/*  V4L2 camera open / setup / close                                   */
/* ------------------------------------------------------------------ */
static int xioctl(int fd, unsigned long req, void *arg)
{
    int r;
    do { r = ioctl(fd, req, arg); } while (r == -1 && errno == EINTR);
    return r;
}

int open_camera(const StreamConfig *cfg, MmapBuffer **bufs_out, int *n_bufs_out)
{
    int fd = open(cfg->v4l2_dev, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        log_message("ERROR: open %s: %s", cfg->v4l2_dev, strerror(errno));
        return -1;
    }

    /*
     * Request YUYV (raw YUV 4:2:2).  The GPU just passes sensor pixels
     * through — no MMAL/VCHIQ encoding traffic, so VCHIQ stays quiet.
     * We compress to JPEG on the CPU in yuyv_to_jpeg().
     */
    struct v4l2_format fmt = {0};
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = (unsigned)cfg->width;
    fmt.fmt.pix.height      = (unsigned)cfg->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;
    if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        log_message("ERROR: VIDIOC_S_FMT YUYV: %s", strerror(errno));
        close(fd);
        return -1;
    }

    /* Set framerate */
    struct v4l2_streamparm parm = {0};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator   = 1;
    parm.parm.capture.timeperframe.denominator = (unsigned)cfg->fps;
    xioctl(fd, VIDIOC_S_PARM, &parm);

    /* Request MMAP buffers */
    struct v4l2_requestbuffers req = {0};
    req.count  = N_BUFFERS;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        log_message("ERROR: VIDIOC_REQBUFS: %s", strerror(errno));
        close(fd);
        return -1;
    }

    MmapBuffer *bufs = calloc(req.count, sizeof(MmapBuffer));
    if (!bufs) { close(fd); return -1; }

    unsigned i;
    for (i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {0};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            log_message("ERROR: VIDIOC_QUERYBUF: %s", strerror(errno));
            free(bufs); close(fd); return -1;
        }
        bufs[i].length = buf.length;
        bufs[i].start  = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, buf.m.offset);
        if (bufs[i].start == MAP_FAILED) {
            log_message("ERROR: mmap: %s", strerror(errno));
            free(bufs); close(fd); return -1;
        }
    }

    /* Enqueue all buffers */
    for (i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {0};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            log_message("ERROR: VIDIOC_QBUF: %s", strerror(errno));
            free(bufs); close(fd); return -1;
        }
    }

    /* Start streaming */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        log_message("ERROR: VIDIOC_STREAMON: %s", strerror(errno));
        free(bufs); close(fd); return -1;
    }

    *bufs_out   = bufs;
    *n_bufs_out = (int)req.count;
    log_message("Camera open: %s %dx%d @%dfps YUYV (%d buffers)",
                cfg->v4l2_dev, cfg->width, cfg->height, cfg->fps, req.count);
    return fd;
}

void close_camera(int fd, MmapBuffer *bufs, int n_bufs)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(fd, VIDIOC_STREAMOFF, &type);
    for (int i = 0; i < n_bufs; i++)
        munmap(bufs[i].start, bufs[i].length);
    free(bufs);
    close(fd);
}

/* ------------------------------------------------------------------ */
/*  libcurl helpers                                                     */
/* ------------------------------------------------------------------ */
static size_t write_cb(void *c, size_t s, size_t n, void *u)
{ (void)c; (void)u; return s * n; }

static struct curl_slist *make_api_headers(const char *api_key)
{
    char hdr[320];
    snprintf(hdr, sizeof(hdr), "X-API-Key: %s", api_key);
    return curl_slist_append(NULL, hdr);
}

/* Post a JPEG buffer; reuses the provided CURL handle (caller owns it). */
static int post_jpeg(CURL *curl, struct curl_slist *headers,
                     const char *url, const char *device_id,
                     const unsigned char *data, size_t size,
                     const char *field, const char *filename)
{
    curl_mime     *form = curl_mime_init(curl);
    curl_mimepart *part;

    part = curl_mime_addpart(form);
    curl_mime_name(part, field);
    curl_mime_filename(part, filename);
    curl_mime_type(part, "image/jpeg");
    curl_mime_data(part, (const char *)data, size);

    part = curl_mime_addpart(form);
    curl_mime_name(part, "device_id");
    curl_mime_data(part, device_id, CURL_ZERO_TERMINATED);

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL,           url);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST,      form);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       3L);

    CURLcode res = curl_easy_perform(curl);
    int ok = 0;
    if (res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        ok = (code == 200 || code == 201);
    }
    curl_mime_free(form);
    return ok ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/*  Offline frame storage — circular buffer on the local filesystem    */
/* ------------------------------------------------------------------ */

static int offline_disk_ok(void)
{
    struct statvfs vfs;
    if (statvfs("/var/spool", &vfs) != 0) return 1;
    unsigned long long free_bytes = (unsigned long long)vfs.f_bavail
                                  * (unsigned long long)vfs.f_bsize;
    return free_bytes >= OFFLINE_MIN_FREE_BYTES;
}

static int offline_frames_ensure_dir(void)
{
    struct stat st;
    if (stat(OFFLINE_FRAMES_DIR, &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
    return mkdir(OFFLINE_FRAMES_DIR, 0755);
}

static int cmp_str_ptr(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

/* Returns number of .jpg files found, fills names[] (caller frees each entry). */
static int list_offline_frames(char **names, int max)
{
    DIR *dir = opendir(OFFLINE_FRAMES_DIR);
    if (!dir) return 0;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && count < max) {
        size_t nlen = strlen(ent->d_name);
        if (nlen < 4 || strcmp(ent->d_name + nlen - 4, ".jpg") != 0) continue;
        names[count] = strdup(ent->d_name);
        if (names[count]) count++;
    }
    closedir(dir);
    qsort(names, count, sizeof(char *), cmp_str_ptr);
    return count;
}

static void offline_frames_trim(void)
{
    char *names[MAX_OFFLINE_FRAMES + 64];
    int count = list_offline_frames(names, MAX_OFFLINE_FRAMES + 64);
    if (count <= MAX_OFFLINE_FRAMES) {
        for (int i = 0; i < count; i++) free(names[i]);
        return;
    }
    int to_delete = count - MAX_OFFLINE_FRAMES;
    char path[512];
    for (int i = 0; i < count; i++) {
        if (i < to_delete) {
            snprintf(path, sizeof(path), "%s/%s", OFFLINE_FRAMES_DIR, names[i]);
            unlink(path);
        }
        free(names[i]);
    }
    log_message("offline_frames_trim: removed %d oldest frames (kept %d)",
                to_delete, MAX_OFFLINE_FRAMES);
}

static int offline_frames_store(const unsigned char *data, size_t size)
{
    if (offline_frames_ensure_dir() != 0) return 0;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    char path[512];
    snprintf(path, sizeof(path), "%s/%ld%09ld.jpg",
             OFFLINE_FRAMES_DIR, (long)ts.tv_sec, (long)ts.tv_nsec);

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        log_message("offline_frames_store: cannot create %s: %s", path, strerror(errno));
        return 0;
    }
    fwrite(data, 1, size, fp);
    fclose(fp);
    log_message("Stored offline frame (%zu B): %s", size, path);
    return 1;
}

/* Sends up to max_batch stored frames; stops on first POST failure. */
static void offline_frames_drain_batch(const StreamConfig *cfg,
                                        CURL *curl,
                                        struct curl_slist *hdrs,
                                        int max_batch)
{
    char *names[MAX_OFFLINE_FRAMES + 64];
    int count = list_offline_frames(names, MAX_OFFLINE_FRAMES + 64);
    if (count == 0) return;

    int sent = 0;
    char path[512];
    for (int i = 0; i < count && sent < max_batch; i++) {
        snprintf(path, sizeof(path), "%s/%s", OFFLINE_FRAMES_DIR, names[i]);

        FILE *fp = fopen(path, "rb");
        if (!fp) { free(names[i]); continue; }

        fseek(fp, 0, SEEK_END);
        long fsz = ftell(fp);
        rewind(fp);
        unsigned char *buf = malloc((size_t)fsz);
        if (!buf) { fclose(fp); free(names[i]); continue; }
        fread(buf, 1, (size_t)fsz, fp);
        fclose(fp);

        int ok = post_jpeg(curl, hdrs, cfg->server_stream_url,
                           cfg->device_id, buf, (size_t)fsz, "frame", "frame.jpg");
        free(buf);

        if (ok == 0) {
            unlink(path);
            sent++;
        } else {
            free(names[i]);
            for (int j = i + 1; j < count; j++) free(names[j]);
            break;
        }
        free(names[i]);
    }

    if (sent > 0)
        log_message("offline_frames_drain: sent=%d, remaining=%d",
                    sent, count - sent);
}

/* ------------------------------------------------------------------ */
/*  Push thread — sends latest JPEG to server as fast as it arrives    */
/* ------------------------------------------------------------------ */
typedef struct { const StreamConfig *cfg; } PushArg;

static void *push_thread(void *arg)
{
    const StreamConfig *cfg  = ((PushArg *)arg)->cfg;
    struct curl_slist  *hdrs = make_api_headers(cfg->api_key);

    CURL *curl_stream = curl_easy_init();

    while (running) {
        pthread_mutex_lock(&frame_buf.lock);
        while (!frame_buf.ready && running)
            pthread_cond_wait(&frame_buf.cond, &frame_buf.lock);

        if (!running) { pthread_mutex_unlock(&frame_buf.lock); break; }

        /* Copy frame under lock to keep the critical section short */
        size_t         sz   = frame_buf.size;
        unsigned char *copy = malloc(sz);
        if (copy) memcpy(copy, frame_buf.data, sz);
        frame_buf.ready = 0;
        pthread_mutex_unlock(&frame_buf.lock);

        if (!copy) continue;

        int ok = -1;
        if (curl_stream)
            ok = post_jpeg(curl_stream, hdrs, cfg->server_stream_url,
                           cfg->device_id, copy, sz, "frame", "frame.jpg");

        if (ok == 0) {
            /* Live send succeeded — drain stored offline frames (batch of 5) */
            offline_frames_drain_batch(cfg, curl_stream, hdrs, 5);
        } else if (copy) {
            /* Server unreachable — store frame locally if space allows */
            if (offline_disk_ok()) {
                offline_frames_trim();
                offline_frames_store(copy, sz);
            } else {
                log_message("push_thread: disk full, dropping offline frame");
            }
        }

        free(copy);
    }

    if (curl_stream) curl_easy_cleanup(curl_stream);
    curl_slist_free_all(hdrs);
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Camera reopen with exponential backoff                             */
/* ------------------------------------------------------------------ */
static int reopen_camera(int old_fd, MmapBuffer *old_bufs, int old_n,
                         const StreamConfig *cfg,
                         MmapBuffer **bufs_out, int *n_bufs_out,
                         int attempt)
{
    log_message("Reopening camera (attempt %d)...", attempt + 1);
    close_camera(old_fd, old_bufs, old_n);

    int wait = 1 << attempt;  /* 1s, 2s, 4s, 8s... */
    if (wait > 16) wait = 16;
    sleep((unsigned)wait);

    int fd = open_camera(cfg, bufs_out, n_bufs_out);
    if (fd < 0)
        log_message("Reopen failed (attempt %d), will retry", attempt + 1);
    return fd;
}

/* ------------------------------------------------------------------ */
/*  Main — capture loop with stall detection and auto-recovery         */
/* ------------------------------------------------------------------ */
int main(void)
{
    StreamConfig cfg;
    MmapBuffer  *bufs   = NULL;
    int          n_bufs = 0;
    int          cam_fd = -1;

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP,  signal_handler);

    curl_global_init(CURL_GLOBAL_ALL);
    load_config(&cfg);

    frame_buf.data  = NULL;
    frame_buf.size  = 0;
    frame_buf.ready = 0;
    pthread_mutex_init(&frame_buf.lock, NULL);
    pthread_cond_init(&frame_buf.cond, NULL);

    cam_fd = open_camera(&cfg, &bufs, &n_bufs);
    if (cam_fd < 0) { curl_global_cleanup(); return 1; }

    PushArg   push_arg = { .cfg = &cfg };
    pthread_t push_tid;
    pthread_create(&push_tid, NULL, push_thread, &push_arg);

    log_message("camera-stream: YUYV@%dfps %dx%d → CPU JPEG q%d, snap every %ds",
                cfg.fps, cfg.width, cfg.height,
                cfg.jpeg_quality, cfg.snapshot_interval);

    /*
     * Stall detection: if select() times out STALL_MAX_TIMEOUTS consecutive
     * times (= STALL_MAX_TIMEOUTS * 2 seconds with no frame), the driver
     * has stalled and we reopen the camera.
     */
    const int STALL_MAX_TIMEOUTS = 5;  /* 10s with no frame → reopen */
    const int MAX_REOPEN_ATTEMPTS = 5; /* give up after this many failures */
    int consecutive_timeouts = 0;
    int reopen_attempts      = 0;

    while (running) {
        if (reload_flag) {
            close_camera(cam_fd, bufs, n_bufs);
            load_config(&cfg);
            cam_fd = open_camera(&cfg, &bufs, &n_bufs);
            if (cam_fd < 0) break;
            reload_flag          = 0;
            consecutive_timeouts = 0;
            reopen_attempts      = 0;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(cam_fd, &fds);
        struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
        int r = select(cam_fd + 1, &fds, NULL, NULL, &tv);

        if (r == -1 && errno == EINTR) continue;

        if (r == 0) {
            /* select() timeout — camera produced no frame */
            if (++consecutive_timeouts >= STALL_MAX_TIMEOUTS) {
                log_message("Camera stalled (%ds with no frame), recovering",
                            consecutive_timeouts * 2);
                cam_fd = reopen_camera(cam_fd, bufs, n_bufs, &cfg,
                                       &bufs, &n_bufs, reopen_attempts);
                if (cam_fd < 0) {
                    if (++reopen_attempts >= MAX_REOPEN_ATTEMPTS) {
                        log_message("Camera unrecoverable after %d attempts, exiting",
                                    MAX_REOPEN_ATTEMPTS);
                        break;
                    }
                    /* reopen_camera already slept; keep trying next iteration */
                    consecutive_timeouts = STALL_MAX_TIMEOUTS; /* trigger retry */
                } else {
                    consecutive_timeouts = 0;
                    reopen_attempts      = 0;
                }
            }
            continue;
        }

        if (r < 0) continue;

        /* Dequeue the ready buffer */
        struct v4l2_buffer buf = {0};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(cam_fd, VIDIOC_DQBUF, &buf) < 0) {
            if (errno == EAGAIN) continue;
            log_message("VIDIOC_DQBUF error: %s — recovering", strerror(errno));
            cam_fd = reopen_camera(cam_fd, bufs, n_bufs, &cfg,
                                   &bufs, &n_bufs, reopen_attempts);
            if (cam_fd < 0) {
                if (++reopen_attempts >= MAX_REOPEN_ATTEMPTS) break;
                consecutive_timeouts = STALL_MAX_TIMEOUTS;
            } else {
                consecutive_timeouts = 0;
                reopen_attempts      = 0;
            }
            continue;
        }

        consecutive_timeouts = 0;
        reopen_attempts      = 0;

        /* Convert YUYV → JPEG on the CPU */
        unsigned char *jpeg_data = NULL;
        size_t jpeg_size = yuyv_to_jpeg(bufs[buf.index].start,
                                        cfg.width, cfg.height,
                                        cfg.jpeg_quality, &jpeg_data);

        /* Re-enqueue the V4L2 buffer immediately (before any blocking work) */
        if (xioctl(cam_fd, VIDIOC_QBUF, &buf) < 0)
            log_message("WARN: VIDIOC_QBUF: %s", strerror(errno));

        if (jpeg_size == 0 || !jpeg_data) {
            free(jpeg_data);
            continue;
        }

        /* Hand the JPEG buffer to push_thread (transfer ownership) */
        pthread_mutex_lock(&frame_buf.lock);
        free(frame_buf.data);       /* discard previous frame */
        frame_buf.data  = jpeg_data;
        frame_buf.size  = jpeg_size;
        frame_buf.ready = 1;
        pthread_cond_signal(&frame_buf.cond);
        pthread_mutex_unlock(&frame_buf.lock);
    }

    running = 0;
    pthread_cond_broadcast(&frame_buf.cond);
    pthread_join(push_tid, NULL);

    if (cam_fd >= 0)
        close_camera(cam_fd, bufs, n_bufs);
    free(frame_buf.data);
    pthread_mutex_destroy(&frame_buf.lock);
    pthread_cond_destroy(&frame_buf.cond);
    curl_global_cleanup();
    if (log_fp) fclose(log_fp);

    log_message("camera-stream stopped");
    return 0;
}

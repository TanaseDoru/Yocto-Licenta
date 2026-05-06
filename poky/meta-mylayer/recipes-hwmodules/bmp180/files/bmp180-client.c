#include "bmp180.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH     "/tmp/sensor_data.sock"
#define DEFAULT_I2C_DEV "/dev/i2c-1"
#define DEFAULT_ADDR    BMP180_I2C_ADDR       /* 0x77 */
#define DEFAULT_OSS     BMP180_OSS_STANDARD
#define SEND_INTERVAL   10                    /* seconds between readings */
#define CONFIG_PATH     "/etc/bmp180.conf"

static volatile sig_atomic_t running = 1;
static FILE *log_fp = NULL;

/* ------------------------------------------------------------------ */
/* Signal handler                                                       */
/* ------------------------------------------------------------------ */

static void signal_handler(int signum)
{
    (void)signum;
    running = 0;
}

/* ------------------------------------------------------------------ */
/* Logging                                                              */
/* ------------------------------------------------------------------ */

static void log_message(const char *fmt, ...)
{
    va_list args;

    if (!log_fp) {
        log_fp = fopen("/var/log/bmp180-client.log", "a");
    }

    va_start(args, fmt);
    if (log_fp) {
        vfprintf(log_fp, fmt, args);
        fputc('\n', log_fp);
        fflush(log_fp);
    } else {
        vfprintf(stderr, fmt, args);
        fputc('\n', stderr);
    }
    va_end(args);
}

/* ------------------------------------------------------------------ */
/* Config                                                               */
/* Keys recognised in /etc/bmp180.conf:                                 */
/*   I2C_DEV  = /dev/i2c-1                                              */
/*   I2C_ADDR = 0x77                                                    */
/*   OSS      = 0|1|2|3    (oversampling, default 1)                   */
/* ------------------------------------------------------------------ */

static void read_config(char *i2c_dev, size_t i2c_dev_len,
                         uint8_t *addr, bmp180_oss_t *oss)
{
    FILE *fp = fopen(CONFIG_PATH, "r");
    char  line[128];

    if (!fp) return;

    while (fgets(line, sizeof(line), fp)) {
        char *eq;
        char *key;
        char *val;
        size_t len;

        if (line[0] == '#' || line[0] == '\n') continue;

        eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        key = line;
        val = eq + 1;
        len = strcspn(val, "\r\n");
        val[len] = '\0';

        if (strcmp(key, "I2C_DEV") == 0) {
            strncpy(i2c_dev, val, i2c_dev_len - 1);
            i2c_dev[i2c_dev_len - 1] = '\0';
        } else if (strcmp(key, "I2C_ADDR") == 0) {
            unsigned long parsed = strtoul(val, NULL, 0);
            if (parsed <= 0x7F) *addr = (uint8_t)parsed;
        } else if (strcmp(key, "OSS") == 0) {
            unsigned long parsed = strtoul(val, NULL, 0);
            if (parsed <= 3) *oss = (bmp180_oss_t)parsed;
        }
    }

    fclose(fp);
}

/* ------------------------------------------------------------------ */
/* Send one sensor value to data-collector                              */
/* Format on the socket: "sensor_name:value\0"                         */
/* ------------------------------------------------------------------ */

static int send_value(int sock_fd, const char *name, float value)
{
    struct sockaddr_un addr;
    char buf[128];
    int  len;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    len = snprintf(buf, sizeof(buf), "%s:%.2f", name, value);
    if (len < 0) return -1;

    if (sendto(sock_fd, buf, (size_t)len, 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_message("sendto failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    bmp180_dev_t dev;
    int          sock_fd;
    float        temp_c;
    float        pressure_pa;
    char         i2c_dev[64] = DEFAULT_I2C_DEV;
    uint8_t      addr        = DEFAULT_ADDR;
    bmp180_oss_t oss         = DEFAULT_OSS;

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    read_config(i2c_dev, sizeof(i2c_dev), &addr, &oss);

    log_message("Starting bmp180-client on %s addr 0x%02x oss=%d",
                i2c_dev, addr, (int)oss);

    if (bmp180_open(i2c_dev, addr, oss, &dev) != 0) {
        log_message("bmp180-client: failed to open sensor: %s",
                    strerror(errno));
        return 1;
    }

    sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        log_message("socket failed: %s", strerror(errno));
        bmp180_close(&dev);
        return 1;
    }

    while (running) {

        /* --- Temperature --- */
        if (bmp180_read_temperature(&dev, &temp_c) == 0) {
            log_message("bmp180_temp=%.2f", temp_c);
            send_value(sock_fd, "bmp180_temp", temp_c);
        } else {
            log_message("read temperature failed: %s", strerror(errno));
        }

        /* --- Pressure (Pa) ---
         * bmp180_read_pressure() performs its own internal temperature read
         * as required by the compensation formula, so the two calls are
         * fully independent. */
        if (bmp180_read_pressure(&dev, &pressure_pa) == 0) {
            log_message("bmp180_pressure_pa=%.2f", pressure_pa);
            send_value(sock_fd, "bmp180_pressure_pa", pressure_pa);
        } else {
            log_message("read pressure failed: %s", strerror(errno));
        }

        sleep(SEND_INTERVAL);
    }

    close(sock_fd);
    bmp180_close(&dev);

    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }

    return 0;
}
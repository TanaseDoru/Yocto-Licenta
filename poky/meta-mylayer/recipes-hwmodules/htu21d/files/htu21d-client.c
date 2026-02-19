#include "htu21d.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/sensor_data.sock"
#define DEFAULT_I2C_DEV "/dev/i2c-1"
#define DEFAULT_ADDR 0x40
#define SEND_INTERVAL 10
#define CONFIG_PATH "/etc/htu21d.conf"

static volatile sig_atomic_t running = 1;
static FILE *log_fp = NULL;

static void signal_handler(int signum) {
    (void)signum;
    running = 0;
}

static void log_message(const char *fmt, ...) {
    va_list args;

    if (!log_fp) {
        log_fp = fopen("/var/log/htu21d-client.log", "a");
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

static void read_config(char *i2c_dev, size_t i2c_dev_len, uint8_t *addr) {
    FILE *fp = fopen(CONFIG_PATH, "r");
    char line[128];

    if (!fp) {
        return;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *eq;
        char *key;
        char *val;
        size_t len;

        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        eq = strchr(line, '=');
        if (!eq) {
            continue;
        }

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
            if (parsed <= 0x7f) {
                *addr = (uint8_t)parsed;
            }
        }
    }

    fclose(fp);
}

static int send_value(int sock_fd, const char *name, float value) {
    struct sockaddr_un addr;
    char buf[128];
    int len;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    len = snprintf(buf, sizeof(buf), "%s:%.2f", name, value);
    if (len < 0) {
        return -1;
    }

    if (sendto(sock_fd, buf, (size_t)len, 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_message("sendto failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int main(void) {
    int fd;
    int sock_fd;
    float temp_c;
    float rh;
    char i2c_dev[64] = DEFAULT_I2C_DEV;
    uint8_t addr = DEFAULT_ADDR;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    read_config(i2c_dev, sizeof(i2c_dev), &addr);
    log_message("Starting htu21d-client on %s addr 0x%02x", i2c_dev, addr);

    fd = htu21d_open(i2c_dev, addr);
    if (fd < 0) {
        log_message("htu21d-client: failed to open sensor (%s)",
                    strerror(errno));
        return 1;
    }

    sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        log_message("socket failed: %s", strerror(errno));
        htu21d_close(fd);
        return 1;
    }

    while (running) {
        if (htu21d_read_temperature(fd, &temp_c) == 0) {
            log_message("htu21d_temp=%.2f", temp_c);
            send_value(sock_fd, "htu21d_temp", temp_c);
        } else {
            log_message("read temperature failed: %s", strerror(errno));
        }

        if (htu21d_read_humidity(fd, &rh) == 0) {
            log_message("htu21d_rh=%.2f", rh);
            send_value(sock_fd, "htu21d_rh", rh);
        } else {
            log_message("read humidity failed: %s", strerror(errno));
        }

        sleep(SEND_INTERVAL);
    }

    close(sock_fd);
    htu21d_close(fd);
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
    return 0;
}

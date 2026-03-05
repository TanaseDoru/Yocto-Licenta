/*
 * dht11.c — Driver DHT11 via /sys/class/gpio
 */

#include "dht11.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define GPIO_PATH       "/sys/class/gpio"
#define BIT_THRESHOLD   16
#define TIMEOUT_US      255

/* -----------------------------------------------------------------------
 * Helpers sysfs
 * ----------------------------------------------------------------------- */

static int sysfs_write(const char *path, const char *val) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[DHT11] sysfs_write open(%s) failed: %s\n",
                path, strerror(errno));
        return -1;
    }
    ssize_t r = write(fd, val, strlen(val));
    int saved = errno;
    close(fd);
    if (r < 0) {
        fprintf(stderr, "[DHT11] sysfs_write write(%s, %s) failed: %s\n",
                path, val, strerror(saved));
        return -1;
    }
    return 0;
}

static void gpio_export(unsigned pin) {
    char path[64], buf[8];
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%u", pin);

    /* Deja exportat? */
    if (access(path, F_OK) == 0) {
        fprintf(stderr, "[DHT11] GPIO%u deja exportat\n", pin);
        return;
    }

    snprintf(buf, sizeof(buf), "%u", pin);
    sysfs_write(GPIO_PATH "/export", buf);

    /* Asteapta pana apare directorul (max 500ms) */
    for (int i = 0; i < 50; i++) {
        if (access(path, F_OK) == 0) {
            fprintf(stderr, "[DHT11] GPIO%u exportat dupa %dms\n", pin, i*10);
            return;
        }
        usleep(10000);
    }
    fprintf(stderr, "[DHT11] WARN: directorul %s nu a aparut dupa export!\n", path);
}

static void gpio_unexport(unsigned pin) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", pin);
    sysfs_write(GPIO_PATH "/unexport", buf);
}

static void gpio_set_direction(unsigned pin, const char *dir) {
    char path[64];
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%u/direction", pin);
    if (sysfs_write(path, dir) == 0)
        fprintf(stderr, "[DHT11] GPIO%u direction=%s\n", pin, dir);
}

static void gpio_write_val(unsigned pin, int val) {
    char path[64];
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%u/value", pin);
    sysfs_write(path, val ? "1" : "0");
}

static int gpio_open_value(unsigned pin) {
    char path[64];
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%u/value", pin);

    /* Verifica existenta inainte de open */
    if (access(path, F_OK) != 0) {
        fprintf(stderr, "[DHT11] gpio_open_value: %s nu exista: %s\n",
                path, strerror(errno));
        return -1;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        fprintf(stderr, "[DHT11] gpio_open_value open(%s) failed: %s\n",
                path, strerror(errno));
    return fd;
}

static inline int gpio_read_fd(int fd) {
    char c;
    lseek(fd, 0, SEEK_SET);
    if (read(fd, &c, 1) != 1) return -1;
    return (c == '1') ? 1 : 0;
}

/* -----------------------------------------------------------------------
 * Timing
 * ----------------------------------------------------------------------- */

static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL +
           (uint64_t)(ts.tv_nsec / 1000);
}

static void busy_wait_us(unsigned us) {
    uint64_t end = now_us() + us;
    while (now_us() < end) {}
}

static void busy_wait_ms(unsigned ms) {
    busy_wait_us(ms * 1000u);
}

static uint8_t wait_and_measure(int fd, int level) {
    uint8_t t = 0;
    while (gpio_read_fd(fd) != level) {
        busy_wait_us(1);
        if (++t == TIMEOUT_US) return TIMEOUT_US;
    }
    t = 0;
    while (gpio_read_fd(fd) == level) {
        busy_wait_us(1);
        if (++t == TIMEOUT_US) return TIMEOUT_US;
    }
    return t;
}

/* -----------------------------------------------------------------------
 * API public
 * ----------------------------------------------------------------------- */

int dht11_open(DHT11Dev *dev, unsigned gpio_pin) {
    if (!dev) return -1;

    fprintf(stderr, "[DHT11] dht11_open(GPIO%u)\n", gpio_pin);

    dev->gpio_pin = gpio_pin;
    dev->pi       = 0;

    gpio_export(gpio_pin);
    gpio_set_direction(gpio_pin, "out");
    gpio_write_val(gpio_pin, 1);

    /* Verifica ca pinul e accesibil */
    char path[64];
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%u/value", gpio_pin);
    if (access(path, F_OK) != 0) {
        fprintf(stderr, "[DHT11] dht11_open FAILED: %s inaccesibil\n", path);
        return -1;
    }

    fprintf(stderr, "[DHT11] dht11_open OK\n");
    return 0;
}

void dht11_close(DHT11Dev *dev) {
    if (!dev) return;
    gpio_set_direction(dev->gpio_pin, "in");
    gpio_unexport(dev->gpio_pin);
    dev->pi = -1;
}

int dht11_read(DHT11Dev *dev, DHT11Data *data) {
    if (!dev || !data || dev->pi < 0) return -1;

    unsigned pin = dev->gpio_pin;
    uint8_t  buf[DHT11_BYTES] = {0};
    int      val_fd = -1;
    int      ret    = -1;

    /* START: LOW 20ms -> HIGH 40us -> input */
    gpio_set_direction(pin, "out");
    gpio_write_val(pin, 0);
    busy_wait_ms(20);
    gpio_write_val(pin, 1);
    busy_wait_us(40);
    gpio_set_direction(pin, "in");

    /* Deschide fd dupa ce pinul e in input */
    val_fd = gpio_open_value(pin);
    if (val_fd < 0)
        goto cleanup;

    /* Raspuns senzor: LOW ~80us, HIGH ~80us */
    uint8_t t;
    t = wait_and_measure(val_fd, 0);
    if (t == TIMEOUT_US) { fprintf(stderr, "[DHT11] Timeout LOW raspuns\n"); goto cleanup; }

    t = wait_and_measure(val_fd, 1);
    if (t == TIMEOUT_US) { fprintf(stderr, "[DHT11] Timeout HIGH raspuns\n"); goto cleanup; }

    /* 40 biti */
    for (int i = 0; i < DHT11_BITS; i++) {
        t = wait_and_measure(val_fd, 0);
        if (t == TIMEOUT_US) { fprintf(stderr, "[DHT11] Timeout LOW bit %d\n", i); goto cleanup; }

        t = wait_and_measure(val_fd, 1);
        if (t == TIMEOUT_US) { fprintf(stderr, "[DHT11] Timeout HIGH bit %d\n", i); goto cleanup; }

        buf[i / 8] <<= 1;
        if (t > BIT_THRESHOLD)
            buf[i / 8] |= 1;
    }

    fprintf(stderr,
            "[DHT11] Raw: %d %d %d %d | chk_primit=%d chk_calc=%d\n",
            buf[0], buf[1], buf[2], buf[3], buf[4],
            (buf[0] + buf[1] + buf[2] + buf[3]) & 0xFF);

    if (buf[4] != ((buf[0] + buf[1] + buf[2] + buf[3]) & 0xFF)) {
        fprintf(stderr, "[DHT11] Checksum gresit\n");
        goto cleanup;
    }

    data->humidity_rh   = (float)buf[0] + (float)buf[1] * 0.1f;
    data->temperature_c = (float)buf[2] + (float)buf[3] * 0.1f;
    ret = 0;

cleanup:
    if (val_fd >= 0) close(val_fd);
    return ret;
}
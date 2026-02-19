#include "htu21d.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define HTU21D_ADDR 0x40
#define CMD_TEMP_NOHOLD 0xF3
#define CMD_RH_NOHOLD 0xF5
#define TEMP_DELAY_US 100000
#define RH_DELAY_US 50000

static int htu21d_select(int fd, uint8_t addr) {
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        perror("ioctl(I2C_SLAVE)");
        return -1;
    }
    return 0;
}

#ifndef DEBUG
static uint8_t htu21d_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = (uint8_t)((crc << 1) ^ 0x31);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}
#endif

static int htu21d_read_measurement(int fd, uint8_t cmd, int delay_us, float *out, int is_temp) {
    uint8_t buf[3];
    uint16_t raw;

    if (write(fd, &cmd, 1) != 1) return -1;

    // retry up to ~10 times
    for (int attempt = 0; attempt < 10; attempt++) {
        usleep(delay_us);

        ssize_t r = read(fd, buf, sizeof(buf));
        if (r != (ssize_t)sizeof(buf)) {
            // EIO / not ready -> try again
            continue;
        }

#ifndef DEBUG
        if (htu21d_crc8(buf, 2) != buf[2]) {
            // CRC fail -> try again
            continue;
        }
#endif

        raw = (uint16_t)((buf[0] << 8) | buf[1]);
        raw &= 0xFFFC;

        // guard against the classic "FF FF" style bogus read
        if (raw == 0xFFFC || raw == 0x0000) {
            continue;
        }

        if (is_temp) {
            *out = -46.85f + (175.72f * (float)raw / 65536.0f);
        } else {
            *out = -6.0f + (125.0f * (float)raw / 65536.0f);

            // clamp physical limits (recommended)
            if (*out < 0.0f) *out = 0.0f;
            if (*out > 100.0f) *out = 100.0f;
        }

        return 0;
    }

    errno = EIO;
    return -1;
}


int htu21d_open(const char *i2c_dev, uint8_t addr) {
    int fd = open(i2c_dev, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    if (addr == 0) {
        addr = HTU21D_ADDR;
    }

    if (htu21d_select(fd, addr) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

void htu21d_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int htu21d_read_temperature(int fd, float *out_c) {
    if (!out_c) {
        errno = EINVAL;
        return -1;
    }
    return htu21d_read_measurement(fd, CMD_TEMP_NOHOLD, TEMP_DELAY_US, out_c, 1);
}

int htu21d_read_humidity(int fd, float *out_rh) {
    if (!out_rh) {
        errno = EINVAL;
        return -1;
    }
    return htu21d_read_measurement(fd, CMD_RH_NOHOLD, RH_DELAY_US, out_rh, 0);
}

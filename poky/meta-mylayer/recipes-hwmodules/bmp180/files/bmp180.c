#include "bmp180.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

/* ------------------------------------------------------------------ */
/* Internal register addresses                                          */
/* ------------------------------------------------------------------ */
#define BMP180_REG_CHIP_ID      0xD0   /* should read 0x55             */
#define BMP180_REG_CALIB_START  0xAA   /* 22 bytes of calibration data */
#define BMP180_REG_CTRL_MEAS    0xF4
#define BMP180_REG_OUT_MSB      0xF6
#define BMP180_REG_OUT_LSB      0xF7
#define BMP180_REG_OUT_XLSB     0xF8   /* used for pressure XLSB       */

/* Commands written to CTRL_MEAS */
#define BMP180_CMD_READ_TEMP    0x2E
#define BMP180_CMD_READ_PRES(oss) ((uint8_t)(0x34 | ((oss) << 6)))

/* Conversion wait times in microseconds (from datasheet) */
static const unsigned int oss_delay_us[4] = {
    4500,   /* OSS 0 – ultra low power */
    7500,   /* OSS 1 – standard        */
    13500,  /* OSS 2 – high res        */
    25500   /* OSS 3 – ultra high res  */
};

/* ------------------------------------------------------------------ */
/* Low-level helpers                                                    */
/* ------------------------------------------------------------------ */

static int i2c_read_byte(int fd, uint8_t reg, uint8_t *out)
{
    if (write(fd, &reg, 1) != 1) return -1;
    if (read(fd, out, 1) != 1)   return -1;
    return 0;
}

static int i2c_read_word_be(int fd, uint8_t reg, uint16_t *out)
{
    uint8_t buf[2];
    if (write(fd, &reg, 1) != 1) return -1;
    if (read(fd, buf, 2) != 2)   return -1;
    *out = (uint16_t)((buf[0] << 8) | buf[1]);
    return 0;
}

static int i2c_write_byte(int fd, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    if (write(fd, buf, 2) != 2) return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Calibration                                                          */
/* ------------------------------------------------------------------ */

static int load_calibration(int fd, bmp180_calib_t *c)
{
    /*
     * The BMP180 stores 11 calibration words (22 bytes) starting at 0xAA.
     * Each word is big-endian. Some are signed (int16_t), some unsigned.
     * We read them individually to keep the code explicit and easy to audit.
     */
    struct {
        uint8_t  reg;
        int      is_signed;
        void    *dest;
    } fields[] = {
        { 0xAA, 1, &c->AC1 },
        { 0xAC, 1, &c->AC2 },
        { 0xAE, 1, &c->AC3 },
        { 0xB0, 0, &c->AC4 },
        { 0xB2, 0, &c->AC5 },
        { 0xB4, 0, &c->AC6 },
        { 0xB6, 1, &c->B1  },
        { 0xB8, 1, &c->B2  },
        { 0xBA, 1, &c->MB  },
        { 0xBC, 1, &c->MC  },
        { 0xBE, 1, &c->MD  },
    };
    int n = (int)(sizeof(fields) / sizeof(fields[0]));

    for (int i = 0; i < n; i++) {
        uint16_t raw;
        if (i2c_read_word_be(fd, fields[i].reg, &raw) != 0) return -1;
        /* BMP180 datasheet: calibration words must not be 0x0000 or 0xFFFF */
        if (raw == 0x0000 || raw == 0xFFFF) {
            errno = EIO;
            return -1;
        }
        if (fields[i].is_signed) {
            *(int16_t *)fields[i].dest = (int16_t)raw;
        } else {
            *(uint16_t *)fields[i].dest = raw;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Raw ADC reads                                                        */
/* ------------------------------------------------------------------ */

static int read_raw_temperature(int fd, int32_t *raw_t)
{
    uint16_t msb_lsb;

    if (i2c_write_byte(fd, BMP180_REG_CTRL_MEAS, BMP180_CMD_READ_TEMP) != 0)
        return -1;

    usleep(4500); /* temperature conversion time: 4.5 ms */

    if (i2c_read_word_be(fd, BMP180_REG_OUT_MSB, &msb_lsb) != 0)
        return -1;

    *raw_t = (int32_t)msb_lsb;
    return 0;
}

static int read_raw_pressure(int fd, bmp180_oss_t oss, int32_t *raw_p)
{
    uint8_t msb, lsb, xlsb;

    if (i2c_write_byte(fd, BMP180_REG_CTRL_MEAS,
                       BMP180_CMD_READ_PRES(oss)) != 0)
        return -1;

    usleep(oss_delay_us[oss]);

    if (i2c_read_byte(fd, BMP180_REG_OUT_MSB,  &msb)  != 0) return -1;
    if (i2c_read_byte(fd, BMP180_REG_OUT_LSB,  &lsb)  != 0) return -1;
    if (i2c_read_byte(fd, BMP180_REG_OUT_XLSB, &xlsb) != 0) return -1;

    /* BMP180 datasheet eq. (1): UP = (MSB<<16 + LSB<<8 + XLSB) >> (8 - oss) */
    *raw_p = (int32_t)(
        ((uint32_t)msb  << 16) |
        ((uint32_t)lsb  <<  8) |
         (uint32_t)xlsb
    ) >> (8 - (int)oss);

    return 0;
}

/* ------------------------------------------------------------------ */
/* Compensation – straight from BMP180 datasheet section 4.1.2         */
/* ------------------------------------------------------------------ */

/*
 * Compute B5 (shared intermediate for both temperature and pressure).
 * Returns B5; also writes the true temperature (in 0.1 °C units) to *t_fine
 * if t_fine is non-NULL.
 */
static int32_t compute_b5(const bmp180_calib_t *c, int32_t raw_t)
{
    int32_t X1 = ((raw_t - (int32_t)c->AC6) * (int32_t)c->AC5) >> 15;
    int32_t X2 = ((int32_t)c->MC << 11) / (X1 + (int32_t)c->MD);
    return X1 + X2; /* B5 */
}

static float compensate_temperature(const bmp180_calib_t *c, int32_t raw_t)
{
    int32_t B5 = compute_b5(c, raw_t);
    int32_t T  = (B5 + 8) >> 4;  /* T in units of 0.1 °C */
    return (float)T / 10.0f;
}

static float compensate_pressure(const bmp180_calib_t *c,
                                  int32_t raw_t, int32_t raw_p,
                                  bmp180_oss_t oss)
{
    int32_t B5 = compute_b5(c, raw_t);

    int32_t B6 = B5 - 4000;
    int32_t X1 = ((int32_t)c->B2 * ((B6 * B6) >> 12)) >> 11;
    int32_t X2 = ((int32_t)c->AC2 * B6) >> 11;
    int32_t X3 = X1 + X2;
    int32_t B3 = ((((int32_t)c->AC1 * 4 + X3) << (int)oss) + 2) / 4;

    X1 = ((int32_t)c->AC3 * B6) >> 13;
    X2 = ((int32_t)c->B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    uint32_t B4 = ((uint32_t)c->AC4 * (uint32_t)(X3 + 32768)) >> 15;
    uint32_t B7 = ((uint32_t)raw_p - (uint32_t)B3) * (50000u >> (int)oss);

    int32_t p;
    if (B7 < 0x80000000u) {
        p = (int32_t)((B7 * 2u) / B4);
    } else {
        p = (int32_t)((B7 / B4) * 2u);
    }

    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    p += (X1 + X2 + 3791) >> 4;

    return (float)p; /* Pascals */
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int bmp180_open(const char *i2c_dev, uint8_t addr,
                bmp180_oss_t oss, bmp180_dev_t *dev)
{
    uint8_t chip_id;

    dev->fd  = -1;
    dev->oss = oss;

    dev->fd = open(i2c_dev, O_RDWR);
    if (dev->fd < 0) return -1;

    if (ioctl(dev->fd, I2C_SLAVE, addr) < 0) goto fail;

    /* Verify chip ID: BMP180 always returns 0x55 */
    if (i2c_read_byte(dev->fd, BMP180_REG_CHIP_ID, &chip_id) != 0) goto fail;
    if (chip_id != 0x55) {
        errno = ENODEV;
        goto fail;
    }

    if (load_calibration(dev->fd, &dev->calib) != 0) goto fail;

    return 0;

fail:
    close(dev->fd);
    dev->fd = -1;
    return -1;
}

int bmp180_read_temperature(bmp180_dev_t *dev, float *temp_c)
{
    int32_t raw_t;
    if (read_raw_temperature(dev->fd, &raw_t) != 0) return -1;
    *temp_c = compensate_temperature(&dev->calib, raw_t);
    return 0;
}

int bmp180_read_pressure(bmp180_dev_t *dev, float *pa)
{
    int32_t raw_t, raw_p;
    /* Temperature read is mandatory: its result feeds into B5 used by
     * the pressure compensation formula (BMP180 datasheet §4.1.2). */
    if (read_raw_temperature(dev->fd, &raw_t) != 0) return -1;
    if (read_raw_pressure(dev->fd, dev->oss, &raw_p) != 0) return -1;
    *pa = compensate_pressure(&dev->calib, raw_t, raw_p, dev->oss);
    return 0;
}

void bmp180_close(bmp180_dev_t *dev)
{
    if (dev && dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
}
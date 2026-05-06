#ifndef BMP180_H
#define BMP180_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* I2C address                                                          */
/* ------------------------------------------------------------------ */
#define BMP180_I2C_ADDR     0x77

/* ------------------------------------------------------------------ */
/* Oversampling settings (OSS)                                          */
/*   BMP180_OSS_ULTRA_LOW_POWER  – 1 sample,  4.5 ms conversion        */
/*   BMP180_OSS_STANDARD         – 2 samples, 7.5 ms conversion         */
/*   BMP180_OSS_HIGH_RES         – 4 samples, 13.5 ms conversion        */
/*   BMP180_OSS_ULTRA_HIGH_RES   – 8 samples, 25.5 ms conversion        */
/* ------------------------------------------------------------------ */
typedef enum {
    BMP180_OSS_ULTRA_LOW_POWER = 0,
    BMP180_OSS_STANDARD        = 1,
    BMP180_OSS_HIGH_RES        = 2,
    BMP180_OSS_ULTRA_HIGH_RES  = 3
} bmp180_oss_t;

/* ------------------------------------------------------------------ */
/* Calibration coefficients – read once from sensor EEPROM at open()   */
/* ------------------------------------------------------------------ */
typedef struct {
    int16_t  AC1;
    int16_t  AC2;
    int16_t  AC3;
    uint16_t AC4;
    uint16_t AC5;
    uint16_t AC6;
    int16_t  B1;
    int16_t  B2;
    int16_t  MB;
    int16_t  MC;
    int16_t  MD;
} bmp180_calib_t;

/* ------------------------------------------------------------------ */
/* Device handle                                                        */
/* ------------------------------------------------------------------ */
typedef struct {
    int           fd;        /* open file descriptor for /dev/i2c-X  */
    bmp180_calib_t calib;    /* calibration data loaded at open       */
    bmp180_oss_t  oss;       /* oversampling setting used for reads   */
} bmp180_dev_t;

/* ------------------------------------------------------------------ */
/* API                                                                  */
/* ------------------------------------------------------------------ */

/**
 * Open the I2C device, set the slave address and load calibration data.
 *
 * @param i2c_dev  Path to I2C bus, e.g. "/dev/i2c-1"
 * @param addr     7-bit I2C address (usually BMP180_I2C_ADDR = 0x77)
 * @param oss      Oversampling setting for pressure measurements
 * @param dev      Output handle, filled on success
 * @return  0 on success, -1 on error (errno is set)
 */
int bmp180_open(const char *i2c_dev, uint8_t addr,
                bmp180_oss_t oss, bmp180_dev_t *dev);

/**
 * Read compensated temperature.
 *
 * @param dev     Initialised device handle
 * @param temp_c  Output: temperature in degrees Celsius
 * @return  0 on success, -1 on error
 */
int bmp180_read_temperature(bmp180_dev_t *dev, float *temp_c);

/**
 * Read compensated pressure.
 * Internally also reads temperature (required by the BMP180 datasheet
 * compensation formula).
 *
 * @param dev   Initialised device handle
 * @param pa    Output: pressure in Pascal
 * @return  0 on success, -1 on error
 */
int bmp180_read_pressure(bmp180_dev_t *dev, float *pa);

/**
 * Close the I2C file descriptor.
 */
void bmp180_close(bmp180_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BMP180_H */
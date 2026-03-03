/*
 * gy63.c — Driver SPI pentru MS5611 (modul GY-63)
 *
 * Fix: comanda + citire date in acelasi transfer SPI (CS continuu).
 * MS5611 necesita CS activ pe toata durata comenzii + raspunsului.
 */

#include "gy63.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <linux/spi/spidev.h>
#include <sys/ioctl.h>

#define SPI_SPEED_HZ    1000000U   /* 1 MHz */
#define SPI_MODE        SPI_MODE_3 /* MS5611: CPOL=1, CPHA=1 */
#define SPI_BITS        8

/* -----------------------------------------------------------------------
 * Funcții interne
 * ----------------------------------------------------------------------- */

static void delay_ms(unsigned int ms) {
    struct timespec ts = {
        .tv_sec  = ms / 1000,
        .tv_nsec = (long)(ms % 1000) * 1000000L
    };
    nanosleep(&ts, NULL);
}

/**
 * spi_cmd_read – Trimite `cmd_len` bytes de comandă și citește `rx_len` bytes
 *                de răspuns într-un SINGUR transfer SPI (CS rămâne activ).
 *
 * MS5611 necesită comanda + citirea în același transfer — dacă CS se ridică
 * între comandă și citire, senzorul abandonează operația.
 */
static int spi_cmd_read(int fd, const uint8_t *cmd, size_t cmd_len,
                        uint8_t *rx, size_t rx_len) {
    size_t total = cmd_len + rx_len;
    uint8_t tx[total];
    uint8_t rxbuf[total];

    memset(tx, 0x00, total);
    memset(rxbuf, 0x00, total);

    /* Comanda în primii cmd_len bytes, restul 0x00 (dummy) */
    memcpy(tx, cmd, cmd_len);

    struct spi_ioc_transfer t = {
        .tx_buf        = (unsigned long)tx,
        .rx_buf        = (unsigned long)rxbuf,
        .len           = (uint32_t)total,
        .speed_hz      = SPI_SPEED_HZ,
        .bits_per_word = SPI_BITS,
        .cs_change     = 0,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &t) < 0) {
        return -1;
    }

    /* Răspunsul începe după cmd_len bytes */
    if (rx && rx_len > 0) {
        memcpy(rx, rxbuf + cmd_len, rx_len);
    }

    return 0;
}

/**
 * spi_cmd  – Trimite doar o comandă (fără citire răspuns).
 */
static int spi_cmd(int fd, uint8_t cmd) {
    return spi_cmd_read(fd, &cmd, 1, NULL, 0);
}

static int ms5611_reset(int fd) {
    if (spi_cmd(fd, MS5611_CMD_RESET) < 0) {
        return -1;
    }
    delay_ms(3);
    return 0;
}

static int ms5611_read_prom(int fd, uint16_t prom[MS5611_PROM_COUNT]) {
    for (int i = 0; i < MS5611_PROM_COUNT; i++) {
        uint8_t cmd = MS5611_CMD_PROM_READ(i);
        uint8_t buf[2] = {0, 0};

        if (spi_cmd_read(fd, &cmd, 1, buf, 2) < 0) {
            return -1;
        }
        prom[i] = ((uint16_t)buf[0] << 8) | buf[1];
    }
    return 0;
}

static int ms5611_read_adc(int fd, uint8_t conv_cmd, uint32_t *result) {
    /* Pornește conversia */
    if (spi_cmd(fd, conv_cmd) < 0) {
        return -1;
    }
    delay_ms(MS5611_CONV_DELAY_MS);

    /* Citește ADC — comanda 0x00 + 3 bytes răspuns în același transfer */
    uint8_t cmd = MS5611_CMD_ADC_READ;
    uint8_t buf[3] = {0, 0, 0};

    if (spi_cmd_read(fd, &cmd, 1, buf, 3) < 0) {
        return -1;
    }

    *result = ((uint32_t)buf[0] << 16) |
              ((uint32_t)buf[1] << 8)  |
               (uint32_t)buf[2];
    return 0;
}

/* -----------------------------------------------------------------------
 * API public
 * ----------------------------------------------------------------------- */

int gy63_open(GY63Dev *dev, const char *spi_path) {
    if (!dev || !spi_path) {
        errno = EINVAL;
        return -1;
    }

    dev->spi_fd = open(spi_path, O_RDWR);
    if (dev->spi_fd < 0) {
        return -1;
    }

    uint8_t  mode  = SPI_MODE;
    uint8_t  bits  = SPI_BITS;
    uint32_t speed = SPI_SPEED_HZ;

    if (ioctl(dev->spi_fd, SPI_IOC_WR_MODE, &mode)          < 0 ||
        ioctl(dev->spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(dev->spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        close(dev->spi_fd);
        dev->spi_fd = -1;
        return -1;
    }

    if (ms5611_reset(dev->spi_fd) < 0) {
        close(dev->spi_fd);
        dev->spi_fd = -1;
        return -1;
    }

    if (ms5611_read_prom(dev->spi_fd, dev->prom) < 0) {
        close(dev->spi_fd);
        dev->spi_fd = -1;
        return -1;
    }

    /* Debug PROM */
    fprintf(stderr, "[GY-63] PROM raw dump:\n");
    for (int i = 0; i < MS5611_PROM_COUNT; i++) {
        fprintf(stderr, "  PROM[%d] = %u (0x%04X)\n",
                i, dev->prom[i], dev->prom[i]);
    }

    /* Detectează erori de comunicare */
    int all_zero = 1, all_ff = 1;
    for (int i = 1; i <= 6; i++) {
        if (dev->prom[i] != 0x0000) all_zero = 0;
        if (dev->prom[i] != 0xFFFF) all_ff   = 0;
    }
    if (all_zero) {
        fprintf(stderr, "[GY-63] EROARE: PROM toti 0 -> PS la GND? MISO/MOSI?\n");
        close(dev->spi_fd); dev->spi_fd = -1; errno = EIO; return -1;
    }
    if (all_ff) {
        fprintf(stderr, "[GY-63] EROARE: PROM toti 0xFFFF -> VCC/GND?\n");
        close(dev->spi_fd); dev->spi_fd = -1; errno = EIO; return -1;
    }

    return 0;
}

int gy63_read(GY63Dev *dev, double *pressure_pa, double *temp_c) {
    if (!dev || dev->spi_fd < 0 || !pressure_pa || !temp_c) {
        errno = EINVAL;
        return -1;
    }

    uint32_t D1, D2;

    if (ms5611_read_adc(dev->spi_fd, MS5611_CMD_CONVERT_D1, &D1) < 0) {
        return -1;
    }
    if (ms5611_read_adc(dev->spi_fd, MS5611_CMD_CONVERT_D2, &D2) < 0) {
        return -1;
    }

    fprintf(stderr, "[GY-63] D1(presiune raw)=%u  D2(temp raw)=%u\n", D1, D2);

    /* ----------------------------------------------------------------
     * Algoritmul de compensare din datasheet MS5611 (sectiunea 4.7.1)
     * ---------------------------------------------------------------- */
    uint16_t *C = dev->prom;

    int32_t dT   = (int32_t)D2 - (int32_t)((uint32_t)C[5] << 8);
    int32_t TEMP = 2000 + (int32_t)(((int64_t)dT * C[6]) >> 23);

    int64_t OFF  = ((int64_t)C[2] << 16) + (((int64_t)C[4] * dT) >> 7);
    int64_t SENS = ((int64_t)C[1] << 15) + (((int64_t)C[3] * dT) >> 8);

    /* Compensare temperaturi scazute (sub 20°C) */
    int64_t T2 = 0, OFF2 = 0, SENS2 = 0;
    if (TEMP < 2000) {
        T2    = ((int64_t)dT * dT) >> 31;
        OFF2  = 5LL * ((int64_t)(TEMP - 2000) * (TEMP - 2000)) >> 1;
        SENS2 = 5LL * ((int64_t)(TEMP - 2000) * (TEMP - 2000)) >> 2;
        if (TEMP < -1500) {
            OFF2  += 7LL * (int64_t)(TEMP + 1500) * (TEMP + 1500);
            SENS2 += (11LL * (int64_t)(TEMP + 1500) * (TEMP + 1500)) >> 1;
        }
    }
    TEMP -= T2;
    OFF  -= OFF2;
    SENS -= SENS2;

    int32_t P = (int32_t)((((int64_t)D1 * SENS) >> 21) - OFF) >> 15;

    *temp_c      = TEMP / 100.0;
    *pressure_pa = P;

    return 0;
}

void gy63_close(GY63Dev *dev) {
    if (dev && dev->spi_fd >= 0) {
        close(dev->spi_fd);
        dev->spi_fd = -1;
    }
}
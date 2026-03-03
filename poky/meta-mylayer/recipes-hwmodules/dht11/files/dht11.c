/*
 * dht11.c — Driver userspace bit-banging pentru DHT11
 *
 * Folosește libgpiod (/dev/gpiochip0) pentru acces GPIO fără a scrie
 * direct în /sys/class/gpio (deprecated în kernel >= 5.x).
 *
 * Protocolul DHT11:
 *   1. MCU trage DATA LOW minim 18ms  (semnal start)
 *   2. MCU eliberează DATA (HIGH)     (2-40µs)
 *   3. DHT11 răspunde LOW 80µs + HIGH 80µs
 *   4. DHT11 transmite 40 biți:
 *      - bit 0: LOW 50µs + HIGH 26-28µs
 *      - bit 1: LOW 50µs + HIGH 70µs
 *   5. DHT11 trage DATA LOW 50µs (semnal stop)
 *   Frame: [RH_int][RH_dec][T_int][T_dec][checksum]
 */

#include "dht11.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <gpiod.h>

/* -----------------------------------------------------------------------
 * Funcții interne de timing
 * ----------------------------------------------------------------------- */

static void delay_ms(unsigned int ms) {
    struct timespec ts = {
        .tv_sec  = ms / 1000,
        .tv_nsec = (long)(ms % 1000) * 1000000L
    };
    nanosleep(&ts, NULL);
}

static void delay_us(unsigned int us) {
    struct timespec ts = {
        .tv_sec  = 0,
        .tv_nsec = (long)us * 1000L
    };
    nanosleep(&ts, NULL);
}

/* Returnează timpul curent în microsecunde */
static uint64_t time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000);
}

/* Așteaptă ca pinul să ajungă la valoarea `value`, cu timeout în µs.
 * Returnează durata așteptării în µs, sau -1 la timeout. */
static int wait_for_level(struct gpiod_line *line, int value,
                          unsigned int timeout_us) {
    uint64_t start = time_us();
    while (gpiod_line_get_value(line) != value) {
        if ((time_us() - start) > timeout_us) {
            return -1;
        }
    }
    return (int)(time_us() - start);
}

/* -----------------------------------------------------------------------
 * API public
 * ----------------------------------------------------------------------- */

int dht11_open(DHT11Dev *dev, unsigned gpio_pin) {
    if (!dev) {
        errno = EINVAL;
        return -1;
    }
    dev->gpio_pin     = gpio_pin;
    dev->gpio_chip_fd = -1;
    return 0;
}

int dht11_read(DHT11Dev *dev, DHT11Data *data) {
    if (!dev || !data) {
        errno = EINVAL;
        return -1;
    }

    struct gpiod_chip *chip = NULL;
    struct gpiod_line *line = NULL;
    int ret = -1;

    uint8_t bytes[DHT11_BYTES] = {0};

    /* ----------------------------------------------------------------
     * 1. Deschide GPIO chip și obține linia
     * ---------------------------------------------------------------- */
    chip = gpiod_chip_open(DHT11_GPIO_CHIP);
    if (!chip) {
        fprintf(stderr, "[DHT11] gpiod_chip_open(%s) failed: %s\n",
                DHT11_GPIO_CHIP, strerror(errno));
        return -1;
    }

    line = gpiod_chip_get_line(chip, dev->gpio_pin);
    if (!line) {
        fprintf(stderr, "[DHT11] gpiod_chip_get_line(%u) failed: %s\n",
                dev->gpio_pin, strerror(errno));
        goto cleanup;
    }

    /* ----------------------------------------------------------------
     * 2. Semnal START: trage DATA LOW minim 18ms, apoi eliberează
     * ---------------------------------------------------------------- */
    if (gpiod_line_request_output(line, "dht11", 1) < 0) {
        fprintf(stderr, "[DHT11] request_output failed: %s\n", strerror(errno));
        goto cleanup;
    }

    gpiod_line_set_value(line, 0);  /* LOW */
    delay_ms(20);                   /* minim 18ms */
    gpiod_line_set_value(line, 1);  /* HIGH */
    delay_us(40);

    /* Eliberează linia și reconfigurează ca intrare */
    gpiod_line_release(line);
    if (gpiod_line_request_input(line, "dht11") < 0) {
        fprintf(stderr, "[DHT11] request_input failed: %s\n", strerror(errno));
        goto cleanup;
    }

    /* ----------------------------------------------------------------
     * 3. Așteaptă răspunsul DHT11: LOW 80µs + HIGH 80µs
     * ---------------------------------------------------------------- */
    if (wait_for_level(line, 0, DHT11_TIMEOUT_US) < 0) {
        fprintf(stderr, "[DHT11] Timeout asteptand raspuns LOW de la senzor\n");
        fprintf(stderr, "  -> Verificati: firul DATA, rezistenta pull-up 10k\n");
        goto cleanup;
    }
    if (wait_for_level(line, 1, DHT11_TIMEOUT_US) < 0) {
        fprintf(stderr, "[DHT11] Timeout asteptand raspuns HIGH de la senzor\n");
        goto cleanup;
    }
    if (wait_for_level(line, 0, DHT11_TIMEOUT_US) < 0) {
        fprintf(stderr, "[DHT11] Timeout dupa HIGH de raspuns\n");
        goto cleanup;
    }

    /* ----------------------------------------------------------------
     * 4. Citește 40 de biți (5 bytes × 8 biți)
     * ---------------------------------------------------------------- */
    for (int i = 0; i < DHT11_BITS; i++) {
        /* Fiecare bit începe cu LOW ~50µs */
        if (wait_for_level(line, 1, DHT11_TIMEOUT_US) < 0) {
            fprintf(stderr, "[DHT11] Timeout la bitul %d (asteptand HIGH)\n", i);
            goto cleanup;
        }

        /* Măsoară durata HIGH-ului:
         * < 40µs = bit 0
         * > 40µs = bit 1  */
        uint64_t t_start = time_us();
        if (wait_for_level(line, 0, DHT11_TIMEOUT_US) < 0) {
            fprintf(stderr, "[DHT11] Timeout la bitul %d (asteptand LOW)\n", i);
            goto cleanup;
        }
        uint64_t duration = time_us() - t_start;

        /* Shift bit în byte-ul corespunzător (MSB first) */
        int byte_idx = i / 8;
        bytes[byte_idx] <<= 1;
        if (duration > 40) {
            bytes[byte_idx] |= 1;
        }
    }

    /* ----------------------------------------------------------------
     * 5. Verifică checksum
     * Frame: [RH_int][RH_dec][T_int][T_dec][checksum]
     * checksum = (bytes[0]+bytes[1]+bytes[2]+bytes[3]) & 0xFF
     * ---------------------------------------------------------------- */
    uint8_t checksum = (bytes[0] + bytes[1] + bytes[2] + bytes[3]) & 0xFF;
    if (checksum != bytes[4]) {
        fprintf(stderr, "[DHT11] Checksum gresit: calculat=0x%02X primit=0x%02X\n",
                checksum, bytes[4]);
        fprintf(stderr, "  -> Citire corupta, incercati din nou dupa 2s\n");
        goto cleanup;
    }

    /* ----------------------------------------------------------------
     * 6. Decodifică datele
     * DHT11 trimite doar valori întregi (bytes[1] și bytes[3] = 0)
     * ---------------------------------------------------------------- */
    data->humidity_rh    = (float)bytes[0] + (float)bytes[1] * 0.1f;
    data->temperature_c  = (float)bytes[2] + (float)bytes[3] * 0.1f;

    fprintf(stderr, "[DHT11] Raw: RH=%u.%u T=%u.%u checksum=0x%02X OK\n",
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]);

    ret = 0;

cleanup:
    if (line) {
        gpiod_line_release(line);
    }
    if (chip) {
        gpiod_chip_close(chip);
    }
    return ret;
}

void dht11_close(DHT11Dev *dev) {
    /* Resursele sunt eliberate după fiecare citire în dht11_read.
     * Funcția există pentru simetrie cu celelalte drivere. */
    if (dev) {
        dev->gpio_chip_fd = -1;
    }
}
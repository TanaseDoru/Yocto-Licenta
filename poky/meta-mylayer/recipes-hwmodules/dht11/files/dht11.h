#ifndef DHT11_H
#define DHT11_H

/*
 * dht11.h — Driver userspace pentru senzorul DHT11
 *
 * Protocolul DHT11 este 1-wire proprietar (bit-banging).
 * Comunicarea se face prin /dev/gpiochip0 folosind libgpiod (kernel >= 4.8).
 *
 * DHT11 specifcații:
 *   - Temperatura:  0–50°C, precizie ±2°C
 *   - Umiditate:    20–90% RH, precizie ±5% RH
 *   - Interval minim între citiri: 2 secunde
 */

#include <stdint.h>

/* Numărul de biți dintr-un frame DHT11 */
#define DHT11_BITS          40
#define DHT11_BYTES         5

/* Timeouturi în microsecunde */
#define DHT11_TIMEOUT_US    1000

/* GPIO chip device */
#define DHT11_GPIO_CHIP     "/dev/gpiochip0"

typedef struct {
    int      gpio_chip_fd;   /* fd pentru /dev/gpiochip0        */
    unsigned gpio_pin;       /* numărul pinului GPIO (ex: 4)    */
} DHT11Dev;

typedef struct {
    float temperature_c;     /* Temperatura în grade Celsius    */
    float humidity_rh;       /* Umiditatea relativă în %        */
} DHT11Data;

/**
 * dht11_open  – Deschide GPIO chip și configurează pinul.
 * @param dev       Structura dispozitivului
 * @param gpio_pin  Numărul pinului GPIO (ex: 4 pentru GPIO4)
 * @return 0 la succes, -1 la eroare (errno setat)
 */
int dht11_open(DHT11Dev *dev, unsigned gpio_pin);

/**
 * dht11_read  – Citește temperatura și umiditatea de la DHT11.
 *               Apelează cu minim 2 secunde între citiri.
 * @param dev   Structura dispozitivului
 * @param data  Ieșire: temperatura și umiditatea
 * @return 0 la succes, -1 la eroare (timeout / checksum greșit)
 */
int dht11_read(DHT11Dev *dev, DHT11Data *data);

/**
 * dht11_close – Eliberează resursele GPIO.
 */
void dht11_close(DHT11Dev *dev);

#endif /* DHT11_H */
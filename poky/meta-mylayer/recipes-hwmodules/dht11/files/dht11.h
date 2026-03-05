#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>

#define DHT11_BITS   40
#define DHT11_BYTES  5

typedef struct {
    int      pi;        /* handle pigpiod_if2 (pigpio_start) */
    unsigned gpio_pin;  /* BCM GPIO number (ex: 4) */
} DHT11Dev;

typedef struct {
    float temperature_c;
    float humidity_rh;
} DHT11Data;

int  dht11_open(DHT11Dev *dev, unsigned gpio_pin);
int  dht11_read(DHT11Dev *dev, DHT11Data *data);
void dht11_close(DHT11Dev *dev);

#endif
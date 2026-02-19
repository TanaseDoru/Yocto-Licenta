#ifndef HTU21D_H
#define HTU21D_H

#include <stdint.h>

int htu21d_open(const char *i2c_dev, uint8_t addr);
void htu21d_close(int fd);
int htu21d_read_temperature(int fd, float *out_c);
int htu21d_read_humidity(int fd, float *out_rh);

#endif

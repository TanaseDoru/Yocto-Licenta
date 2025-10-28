#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#pragma once
#ifndef DRIVER_DHT11_H
#define DRIVER_DHT11_H
#include "driver_dht11.h"
#endif

static struct gpiod_chip *chip;
static struct gpiod_line *line;

uint8_t dht11_bus_init(void);

uint8_t dht11_bus_write(uint8_t value);

uint8_t dht11_bus_read(uint8_t *value);

uint8_t dht11_bus_deinit(void);

void dht11_delay_ms(uint32_t ms);
void dht11_delay_us(uint32_t us);

void dht11_enable_irq(void);
void dht11_disable_irq(void);

void dht11_debug_print(const char *const fmt, ...);
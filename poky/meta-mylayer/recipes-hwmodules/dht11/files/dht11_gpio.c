#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include "driver_dht11.h"

static struct gpiod_chip *chip;
static struct gpiod_line *line;

uint8_t dht11_bus_init(void)
{
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) return 1;

    line = gpiod_chip_get_line(chip, 4);  // GPIO4
    if (!line) return 1;

    if (gpiod_line_request_output(line, "dht11", 1) < 0)
        return 1;

    return 0;
}

uint8_t dht11_bus_write(uint8_t value)
{
    return gpiod_line_set_value(line, value);
}

uint8_t dht11_bus_read(uint8_t *value)
{
    int val = gpiod_line_get_value(line);
    if (val < 0) return 1;
    *value = val;
    return 0;
}

uint8_t dht11_bus_deinit(void)
{
    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return 0;
}

void dht11_delay_ms(uint32_t ms)
{
    usleep(ms * 1000);
}

void dht11_delay_us(uint32_t us)
{
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

void dht11_enable_irq(void) {}
void dht11_disable_irq(void) {}

void dht11_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

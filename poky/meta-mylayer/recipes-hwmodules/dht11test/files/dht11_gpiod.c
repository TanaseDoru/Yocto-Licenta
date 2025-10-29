#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <gpiod.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>

#define DHT_PIN 4
#define GPIO_CHIP "/dev/gpiochip0"
#define MAX_TIMINGS 85
#define THRESHOLD_US 55
#define TIMEOUT_US 300

static inline long diff_us(struct timespec *t1, struct timespec *t2)
{
    return (t2->tv_sec - t1->tv_sec) * 1000000L +
           (t2->tv_nsec - t1->tv_nsec) / 1000L;
}

static void set_realtime_priority(void)
{
    struct sched_param sp = { .sched_priority = 80 };
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
        perror("sched_setscheduler");
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);               // pin thread to CPU0
    sched_setaffinity(0, sizeof(mask), &mask);
    mlockall(MCL_CURRENT | MCL_FUTURE); // lock memory
}

static int read_dht11_data(float *temp, float *hum)
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    unsigned char data[5] = {0};
    int bit_idx = 0;

    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) return -1;
    line = gpiod_chip_get_line(chip, DHT_PIN);
    if (!line) { gpiod_chip_close(chip); return -1; }

    // Start signal
    gpiod_line_request_output(line, "dht11", 0);
    gpiod_line_set_value(line, 0);
    usleep(18000);
    gpiod_line_set_value(line, 1);
    usleep(40);
    gpiod_line_release(line);
    gpiod_line_request_input(line, "dht11");

    int last = 1, state;
    struct timespec t1, t2;
    long duration;

    // Tight busy-loop reading, no sleeps
    for (int i = 0; i < MAX_TIMINGS * 2; i++) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
        while ((state = gpiod_line_get_value(line)) == last) {
            clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
            duration = diff_us(&t1, &t2);
            if (duration > TIMEOUT_US) break;
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
        duration = diff_us(&t1, &t2);
        last = state;

        if ((i >= 4) && (i % 2 == 0)) {
            data[bit_idx / 8] <<= 1;
            if (duration > THRESHOLD_US)
                data[bit_idx / 8] |= 1;
            bit_idx++;
        }
        if (duration > TIMEOUT_US) break;
    }

    gpiod_line_release(line);
    gpiod_chip_close(chip);

    *hum  = data[0] + data[1] * 0.1;
    *temp = data[2] + data[3] * 0.1;
    return 0;
}

int main(void)
{
    set_realtime_priority();

    float t, h;
    while (1) {
        if (read_dht11_data(&t, &h) == 0)
            printf("Temp: %.1f°C  Humidity: %.1f%%\n", t, h);
        else
            printf("Read failed\n");
        sleep(2);
    }
}

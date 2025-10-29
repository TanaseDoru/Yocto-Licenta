
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <gpiod.h>

#define GPIO_CHIP "/dev/gpiochip0"
#define DHT_PIN   4             // BCM GPIO4 = physical pin 7
#define MAX_TIMINGS 85
#define TIMEOUT_US 200
#define THRESHOLD_US 50

static inline long diff_us(const struct timespec *start, const struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000000L +
           (end->tv_nsec - start->tv_nsec) / 1000L;
}

int read_dht11(float *temp, float *hum)
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    unsigned char data[5] = {0};
    int bit_idx = 0;

    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("gpiod_chip_open");
        return -1;
    }

    line = gpiod_chip_get_line(chip, DHT_PIN);
    if (!line) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return -1;
    }

    // === 1. MCU start signal ===
    gpiod_line_request_output(line, "dht11", 0);
    gpiod_line_set_value(line, 0);
    usleep(18000);               // pull low ≥18ms
    gpiod_line_set_value(line, 1);
    usleep(30);                  // wait 20–40µs
    gpiod_line_release(line);
    gpiod_line_request_input(line, "dht11");

    // === 2. Wait for DHT response ===
    int last_state = 1, state;
    struct timespec t1, t2;
    long duration;

    // We expect about 85 transitions: response + 40 bits
    for (int i = 0; i < MAX_TIMINGS * 2; i++) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
        while ((state = gpiod_line_get_value(line)) == last_state) {
            clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
            duration = diff_us(&t1, &t2);
            if (duration > TIMEOUT_US)
                break;
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
        duration = diff_us(&t1, &t2);
        last_state = state;

        // Ignore preamble transitions (first 4)
        if ((i >= 4) && (i % 2 == 0)) {
            data[bit_idx / 8] <<= 1;
            if (duration > THRESHOLD_US)
                data[bit_idx / 8] |= 1;
            bit_idx++;
        }

        if (bit_idx >= 40)
            break;
    }

    gpiod_line_release(line);
    gpiod_chip_close(chip);

    // === 3. Validate and interpret ===
    int checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (bit_idx < 40) {
        fprintf(stderr, "Incomplete data (%d bits)\n", bit_idx);
        return -1;
    }

    *hum = data[0] + data[1] * 0.1;
    *temp = data[2] + data[3] * 0.1;

    if (data[4] != checksum) {
        fprintf(stderr, "Checksum mismatch (calc=%d recv=%d)\n", checksum, data[4]);
        return -2;
    }

    return 0;
}

int main(void)
{
    printf("DHT11 collector started. Waiting 1s for sensor to stabilize...\n");
    sleep(1);

    float t, h;
    while (1) {
        int res = read_dht11(&t, &h);
        if (res == 0)
            printf("Temperature: %.1f°C  Humidity: %.1f%%\n", t, h);
        else if (res == -2)
            printf("Temperature: %.1f°C  Humidity: %.1f%% (checksum)\n", t, h);
        else
            printf("Read failed\n");

        sleep(1); // Sampling interval ≥1 s per datasheet
    }
    return 0;
}

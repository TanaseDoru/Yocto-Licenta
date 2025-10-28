#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <gpiod.h>

#define DHT_PIN_BCM 4
#define GPIO_CHIP_NAME "gpiochip0"
#define LOG_FILE "dht_log.txt"
#define MAX_TIMINGS 85
#define DATA_BITS 40

static inline void delay_us(long us)
{
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

static inline void delay_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

// Scrierea în fișierul log
void scrie_log(float temp, float hum) {
    FILE *log_fp;
    time_t timer;
    char buffer[26];
    struct tm* tm_info;

    time(&timer);
    tm_info = localtime(&timer);
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) {
        perror("Eroare la deschiderea fisierului de log");
        return;
    }

    fprintf(log_fp, "[%s] Temperatura: %.1f C, Umiditate: %.1f %%\n", buffer, temp, hum);
    fclose(log_fp);
}

int read_dht11_data(float *temp, float *hum) {
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int data[5] = {0};
    int bits[DATA_BITS];
    int checksum;
    int i, j, count;

    printf("[DEBUG] Opening GPIO chip '%s'\n", GPIO_CHIP_NAME);
    chip = gpiod_chip_open_by_name(GPIO_CHIP_NAME);
    if (!chip) {
        printf("[ERROR] Cannot open GPIO chip.\n");
        return -1;
    }

    printf("[DEBUG] Getting line %d\n", DHT_PIN_BCM);
    line = gpiod_chip_get_line(chip, DHT_PIN_BCM);
    if (!line) {
        printf("[ERROR] Cannot get GPIO line %d\n", DHT_PIN_BCM);
        gpiod_chip_close(chip);
        return -1;
    }

    printf("[DEBUG] Requesting line as output\n");
    if (gpiod_line_request_output(line, "dht11", 0) < 0) {
        printf("[ERROR] Failed to request line as output\n");
        gpiod_chip_close(chip);
        return -1;
    }

    printf("[DEBUG] Sending start signal (LOW 18ms)\n");
    gpiod_line_set_value(line, 0);
    delay_ms(18);

    printf("[DEBUG] Pulling HIGH for 40us\n");
    gpiod_line_set_value(line, 1);
    delay_us(40);

    printf("[DEBUG] Switching line to input\n");
    gpiod_line_release(line);
    if (gpiod_line_request_input(line, "dht11") < 0) {
        printf("[ERROR] Failed to request line as input\n");
        gpiod_chip_close(chip);
        return -1;
    }

    printf("[DEBUG] Starting to read %d bits\n", DATA_BITS);
    for (i = 0; i < DATA_BITS; i++) {
        count = 0;
        while (gpiod_line_get_value(line) == 0) {
            if (++count > MAX_TIMINGS) {
                printf("[ERROR] Timeout waiting for HIGH at bit %d\n", i);
                goto cleanup;
            }
            delay_us(1);
        }

        count = 0;
        while (gpiod_line_get_value(line) == 1) {
            if (++count > MAX_TIMINGS) {
                printf("[ERROR] Timeout waiting for LOW at bit %d\n", i);
                goto cleanup;
            }
            delay_us(1);
        }

        bits[i] = (count > 40) ? 1 : 0;
        printf("[DEBUG] Bit %2d = %d (pulse width = %d)\n", i, bits[i], count);
    }

    printf("[DEBUG] Converting bits to bytes\n");
    for (i = 0; i < 5; i++) {
        data[i] = 0;
        for (j = 0; j < 8; j++) {
            if (bits[i * 8 + j]) data[i] |= (1 << (7 - j));
        }
        printf("[DEBUG] Data[%d] = %d\n", i, data[i]);
    }

    checksum = data[0] + data[1] + data[2] + data[3];
    printf("[DEBUG] Checksum calculated = %d, received = %d\n", checksum & 0xFF, data[4]);

    if (data[4] == (checksum & 0xFF)) {
        *hum = data[0] + data[1] / 10.0;
        *temp = data[2] + data[3] / 10.0;
        printf("[INFO] Read successful: T=%.1f, H=%.1f\n", *temp, *hum);
        gpiod_line_release(line);
        gpiod_chip_close(chip);
        return 0;
    } else {
        printf("[ERROR] Invalid checksum!\n");
    }

cleanup:
    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return -1;
}

int main(void) {
    float temperatura, umiditate;
    printf("DHT11 libgpiod collector started. Logging to %s\n", LOG_FILE);

    while (1) {
        if (read_dht11_data(&temperatura, &umiditate) == 0) {
            scrie_log(temperatura, umiditate);
        } else {
            printf("[WARN] Sensor read failed. Retrying...\n");
        }
        sleep(10);
    }
    return 0;
}

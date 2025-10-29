// gpio26_button_monitor.c
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define INPUT_GPIO 26
#define CONSUMER "gpio26_monitor"

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int value, prev_value = -1;

    // Open GPIO chip
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) {
        perror("Failed to open gpiochip0");
        return 1;
    }

    // Get the line for GPIO26
    line = gpiod_chip_get_line(chip, INPUT_GPIO);
    if (!line) {
        perror("Failed to get GPIO line");
        gpiod_chip_close(chip);
        return 1;
    }

    // Request line as input
    if (gpiod_line_request_input(line, CONSUMER) < 0) {
        perror("Failed to request input line");
        gpiod_chip_close(chip);
        return 1;
    }

    printf("Monitoring GPIO%d. Press or release button.\n", INPUT_GPIO);

    while (1) {
        value = gpiod_line_get_value(line);
        if (value < 0) {
            perror("Read line failed");
            break;
        }

        // Only print when state changes
        if (value != prev_value) {
            if (value)
                printf("Button pressed\n");
            else
                printf("Button released\n");

            prev_value = value;
        }

        usleep(50000); // 50ms debounce / prevent flooding
    }

    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return 0;
}

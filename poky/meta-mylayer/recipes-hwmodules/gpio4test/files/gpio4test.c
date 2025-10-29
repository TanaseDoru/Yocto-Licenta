#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

#define GPIO_CHIP "/dev/gpiochip0"
#define GPIO_LINE 4   // BCM4 = physical pin 7

int main(void)
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int value = 0;

    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("gpiod_chip_open");
        return 1;
    }

    line = gpiod_chip_get_line(chip, GPIO_LINE);
    if (!line) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return 1;
    }

    if (gpiod_line_request_output(line, "gpio4_toggle", 0) < 0) {
        perror("gpiod_line_request_output");
        gpiod_chip_close(chip);
        return 1;
    }

    printf("Toggling GPIO4 every second (Ctrl+C to stop)...\n");

    while (1) {
        value = !value;
        if (gpiod_line_set_value(line, value) < 0) {
            perror("gpiod_line_set_value");
            break;
        }
        printf("GPIO4 -> %d\n", value);
        sleep(1);
    }

    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return 0;
}

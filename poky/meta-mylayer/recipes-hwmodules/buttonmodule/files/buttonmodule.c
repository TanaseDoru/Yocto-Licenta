// buttonmodule.c - GPIO26 button monitor with data collector integration
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#define INPUT_GPIO 26
#define CONSUMER "gpio26_monitor"
#define SOCKET_PATH "/tmp/sensor_data.sock"
#define BUFFER_SIZE 256
#define LOG_FILE "button_events.log"

// Send data to the collector daemon
int send_to_collector(const char* sensor_name, float value) {
    int sock;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    snprintf(buffer, BUFFER_SIZE, "%s:%.2f", sensor_name, value);

    if (sendto(sock, buffer, strlen(buffer), 0, 
               (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("sendto");
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

// Write to local log file
void log_button_event(const char* event, int value) {
    FILE* log = fopen(LOG_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(log, "[%s] %s (value: %d)\n", timestamp, event, value);
        fclose(log);
    }
}

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int value, prev_value = -1;
    unsigned long press_count = 0;
    unsigned long release_count = 0;

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
    printf("Sending data to collector at %s\n", SOCKET_PATH);
    printf("Local log: %s\n", LOG_FILE);

    while (1) {
        value = gpiod_line_get_value(line);
        if (value < 0) {
            perror("Read line failed");
            break;
        }

        // Only process when state changes
        if (value != prev_value && prev_value != -1) {
            if (value) {
                // Button pressed
                press_count++;
                printf("Button pressed (total: %lu)\n", press_count);
                log_button_event("Button pressed", value);
                
                // Send button state to collector
                send_to_collector("button_gpio26_press", (float)press_count);
                
            } else {
                // Button released
                release_count++;
                printf("Button released (total: %lu)\n", release_count);
                log_button_event("Button released", value);
                
                // Send button state to collector
                send_to_collector("button_gpio26_release", (float)press_count);
                
            }
        }
        
        prev_value = value;
        usleep(50000); // 50ms debounce
    }

    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return 0;
}
/*
 * dht11-client.c — Citește temperatura și umiditatea de la DHT11
 *                  și le trimite prin Unix domain socket către data-collector.
 *
 * Format mesaje: "dht11_temp:23.00" / "dht11_rh:55.00"
 *
 * Configurare prin /etc/dht11.conf:
 *   GPIO_PIN=4
 *   SEND_INTERVAL=30
 */

#include "dht11.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

/* -----------------------------------------------------------------------
 * Constante implicite
 * ----------------------------------------------------------------------- */
#define SOCKET_PATH      "/tmp/sensor_data.sock"
#define DEFAULT_GPIO_PIN 4
#define DEFAULT_INTERVAL 30    /* DHT11: minim 2s între citiri, 30s practic */
#define CONFIG_PATH      "/etc/dht11.conf"
#define LOG_PATH         "/var/log/dht11-client.log"

/* -----------------------------------------------------------------------
 * Stare globală
 * ----------------------------------------------------------------------- */
static volatile sig_atomic_t running = 1;
static FILE *log_fp = NULL;

/* -----------------------------------------------------------------------
 * Semnale
 * ----------------------------------------------------------------------- */
static void signal_handler(int signum) {
    (void)signum;
    running = 0;
}

/* -----------------------------------------------------------------------
 * Logging
 * ----------------------------------------------------------------------- */
static void log_message(const char *fmt, ...) {
    va_list args;
    time_t now = time(NULL);
    char tbuf[32];
    struct tm *tm_info = localtime(&now);

    if (!log_fp) {
        log_fp = fopen(LOG_PATH, "a");
    }

    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm_info);

    va_start(args, fmt);
    if (log_fp) {
        fprintf(log_fp, "[%s] ", tbuf);
        vfprintf(log_fp, fmt, args);
        fputc('\n', log_fp);
        fflush(log_fp);
    } else {
        fprintf(stderr, "[%s] ", tbuf);
        vfprintf(stderr, fmt, args);
        fputc('\n', stderr);
    }
    va_end(args);
}

/* -----------------------------------------------------------------------
 * Citire configurație
 * ----------------------------------------------------------------------- */
static void read_config(unsigned int *gpio_pin, unsigned int *interval) {
    FILE *fp = fopen(CONFIG_PATH, "r");
    if (!fp) {
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        char *eq = strchr(line, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';

        if (strcmp(key, "GPIO_PIN") == 0) {
            long v = strtol(val, NULL, 10);
            if (v >= 0) {
                *gpio_pin = (unsigned int)v;
            }
        } else if (strcmp(key, "SEND_INTERVAL") == 0) {
            long v = strtol(val, NULL, 10);
            if (v >= 2) {   /* DHT11 necesita minim 2s */
                *interval = (unsigned int)v;
            }
        }
    }
    fclose(fp);
}

/* -----------------------------------------------------------------------
 * Trimitere valoare prin socket
 * ----------------------------------------------------------------------- */
static int send_value(int sock_fd, const char *name, float value) {
    struct sockaddr_un addr;
    char buf[128];

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    int len = snprintf(buf, sizeof(buf), "%s:%.2f", name, value);
    if (len < 0) {
        return -1;
    }

    if (sendto(sock_fd, buf, (size_t)len, 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_message("sendto failed (%s): %s", name, strerror(errno));
        return -1;
    }

    log_message("Sent %s", buf);
    return 0;
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(void) {
    unsigned int gpio_pin = DEFAULT_GPIO_PIN;
    unsigned int interval = DEFAULT_INTERVAL;

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    read_config(&gpio_pin, &interval);

    log_message("Starting dht11-client pe GPIO%u (interval=%us)",
                gpio_pin, interval);

    /* Inițializare senzor */
    DHT11Dev dev;
    if (dht11_open(&dev, gpio_pin) < 0) {
        log_message("ERROR: dht11_open(GPIO%u) failed: %s",
                    gpio_pin, strerror(errno));
        if (log_fp) { fclose(log_fp); }
        return 1;
    }
    log_message("DHT11 inițializat pe GPIO%u", gpio_pin);

    /* Prima citire necesită ~1s warmup */
    sleep(1);

    /* Creare socket Unix */
    int sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        log_message("ERROR: socket() failed: %s", strerror(errno));
        dht11_close(&dev);
        if (log_fp) { fclose(log_fp); }
        return 1;
    }

    /* Buclă principală */
    int consecutive_errors = 0;

    while (running) {
        DHT11Data data;

        if (dht11_read(&dev, &data) == 0) {
            consecutive_errors = 0;

            log_message("dht11_temp=%.1f | dht11_rh=%.1f",
                        data.temperature_c, data.humidity_rh);

            send_value(sock_fd, "dht11_temp", data.temperature_c);
            send_value(sock_fd, "dht11_rh",   data.humidity_rh);
        } else {
            consecutive_errors++;
            log_message("ERROR: citire esuata (incercare %d)", consecutive_errors);

            /* DHT11 e sensibil la timing — după o eroare așteptăm 2s
             * înainte de a reîncerca, indiferent de interval */
            if (consecutive_errors >= 5) {
                log_message("WARN: 5 erori consecutive, reinitializare senzor...");
                dht11_close(&dev);
                sleep(2);
                dht11_open(&dev, gpio_pin);
                consecutive_errors = 0;
            } else {
                sleep(2);
                continue;
            }
        }

        sleep(interval);
    }

    /* Cleanup */
    close(sock_fd);
    dht11_close(&dev);
    log_message("dht11-client oprit.");

    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }

    return 0;
}
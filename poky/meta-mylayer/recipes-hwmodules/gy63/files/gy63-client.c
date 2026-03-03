/*
 * gy63-client.c — Citește presiunea și temperatura de la GY-63 (MS5611)
 *                 prin SPI și le trimite prin Unix domain socket (SOCK_DGRAM)
 *                 către data-collector.
 *
 * Format mesaj: "gy63_press:101325.00" / "gy63_temp:23.45"
 *
 * Configurare opțională prin /etc/gy63.conf:
 *   SPI_DEV=/dev/spidev0.0
 *   SEND_INTERVAL=10
 */

#include "gy63.h"

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
#define DEFAULT_SPI_DEV  "/dev/spidev0.0"
#define DEFAULT_INTERVAL 10          /* secunde între trimiteri */
#define CONFIG_PATH      "/etc/gy63.conf"
#define LOG_PATH         "/var/log/gy63-client.log"

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
static void read_config(char *spi_dev, size_t spi_dev_len,
                        unsigned int *interval) {
    FILE *fp = fopen(CONFIG_PATH, "r");
    if (!fp) {
        return; /* folosim valorile implicite */
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

        if (strcmp(key, "SPI_DEV") == 0) {
            strncpy(spi_dev, val, spi_dev_len - 1);
            spi_dev[spi_dev_len - 1] = '\0';
        } else if (strcmp(key, "SEND_INTERVAL") == 0) {
            long v = strtol(val, NULL, 10);
            if (v > 0) {
                *interval = (unsigned int)v;
            }
        }
    }
    fclose(fp);
}

/* -----------------------------------------------------------------------
 * Trimitere valoare prin socket
 * ----------------------------------------------------------------------- */
static int send_value(int sock_fd, const char *name, double value) {
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
    char spi_dev[64] = DEFAULT_SPI_DEV;
    unsigned int interval = DEFAULT_INTERVAL;

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    read_config(spi_dev, sizeof(spi_dev), &interval);

    log_message("Starting gy63-client on %s (interval=%us)", spi_dev, interval);

    /* Inițializare senzor */
    GY63Dev dev;
    if (gy63_open(&dev, spi_dev) < 0) {
        log_message("ERROR: gy63_open(%s) failed: %s", spi_dev, strerror(errno));
        if (log_fp) { fclose(log_fp); }
        return 1;
    }
    log_message("GY-63 (MS5611) inițializat cu succes pe %s", spi_dev);

    /* Creare socket UDP Unix */
    int sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        log_message("ERROR: socket() failed: %s", strerror(errno));
        gy63_close(&dev);
        if (log_fp) { fclose(log_fp); }
        return 1;
    }

    /* Buclă principală */
    while (running) {
        double pressure_pa, temp_c;

        if (gy63_read(&dev, &pressure_pa, &temp_c) == 0) {
            /* Presiune în hPa (mai ușor de înțeles, 1 hPa = 100 Pa) */
            double pressure_hpa = pressure_pa / 100.0;

            log_message("gy63_press_hpa=%.2f | gy63_temp=%.2f",
                        pressure_hpa, temp_c);

            send_value(sock_fd, "gy63_press_hpa", pressure_hpa);
            send_value(sock_fd, "gy63_temp",      temp_c);
        } else {
            log_message("ERROR: gy63_read() failed: %s", strerror(errno));
        }

        sleep(interval);
    }

    /* Cleanup */
    close(sock_fd);
    gy63_close(&dev);
    log_message("gy63-client oprit.");

    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }

    return 0;
}
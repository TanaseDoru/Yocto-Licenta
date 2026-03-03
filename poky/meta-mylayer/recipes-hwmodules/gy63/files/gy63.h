#ifndef GY63_H
#define GY63_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * GY-63 / MS5611 — SPI driver header
 * Comunicare: SPI mode 0 (CPOL=0, CPHA=0) sau mode 3, max 20 MHz
 * ----------------------------------------------------------------------- */

/* Comenzi MS5611 */
#define MS5611_CMD_RESET        0x1E
#define MS5611_CMD_PROM_READ(n) (0xA0 | ((n) << 1))   /* n = 0..7 */
#define MS5611_CMD_CONVERT_D1   0x48   /* Presiune  — OSR 4096 */
#define MS5611_CMD_CONVERT_D2   0x58   /* Temperatura — OSR 4096 */
#define MS5611_CMD_ADC_READ     0x00

/* Timp de conversie pentru OSR 4096: minim 9.04 ms */
#define MS5611_CONV_DELAY_MS    10

/* Număr de cuvinte PROM (calibrare) */
#define MS5611_PROM_COUNT       8

typedef struct {
    int      spi_fd;                        /* descriptor /dev/spidevX.Y */
    uint16_t prom[MS5611_PROM_COUNT];       /* coeficienți de calibrare  */
} GY63Dev;

/**
 * gy63_open  – Deschide dispozitivul SPI și citește PROM-ul de calibrare.
 * @param spi_path  Calea către dispozitiv, ex: "/dev/spidev0.0"
 * @return 0 la succes, -1 la eroare (errno setat)
 */
int gy63_open(GY63Dev *dev, const char *spi_path);

/**
 * gy63_read  – Citește presiunea (Pa) și temperatura (°C).
 * @param pressure_pa  Ieșire: presiune compensată în Pascal
 * @param temp_c       Ieșire: temperatură compensată în grade Celsius
 * @return 0 la succes, -1 la eroare
 */
int gy63_read(GY63Dev *dev, double *pressure_pa, double *temp_c);

/**
 * gy63_close – Închide descriptorul SPI.
 */
void gy63_close(GY63Dev *dev);

#endif /* GY63_H */
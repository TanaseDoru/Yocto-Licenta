#include "driver_dht11.h"
#include "dht11_gpio.h"
#include <linux/gpio/gpiolib.h>

int main(void)
{
    dht11_handle_t handle;
    uint16_t temp_raw, hum_raw;
    float temp;
    uint8_t hum;

    DRIVER_DHT11_LINK_INIT(&handle, dht11_handle_t);
    DRIVER_DHT11_LINK_BUS_INIT(&handle, dht11_bus_init);
    DRIVER_DHT11_LINK_BUS_DEINIT(&handle, dht11_bus_deinit);
    DRIVER_DHT11_LINK_BUS_READ(&handle, dht11_bus_read);
    DRIVER_DHT11_LINK_BUS_WRITE(&handle, dht11_bus_write);
    DRIVER_DHT11_LINK_DELAY_MS(&handle, dht11_delay_ms);
    DRIVER_DHT11_LINK_DELAY_US(&handle, dht11_delay_us);
    DRIVER_DHT11_LINK_ENABLE_IRQ(&handle, dht11_enable_irq);
    DRIVER_DHT11_LINK_DISABLE_IRQ(&handle, dht11_disable_irq);
    DRIVER_DHT11_LINK_DEBUG_PRINT(&handle, dht11_debug_print);

    if (dht11_init(&handle) != 0)
    {
        printf("DHT11 init failed\n");
        return 1;
    }

    if (dht11_read_temperature_humidity(&handle, &temp_raw, &temp, &hum_raw, &hum) == 0)
        printf("Temp: %.1f°C, Humidity: %d%%\n", temp, hum);
    else
        printf("Failed to read data\n");

    dht11_deinit(&handle);
    return 0;
}

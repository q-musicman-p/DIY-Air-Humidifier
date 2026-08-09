#include <dht.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>

static const char *DHT_TAG = "DHT11";
#define DHT_GPIO gpio_num_t::GPIO_NUM_18
#define DHT_SENSOR_TYPE dht_sensor_type_t::DHT_TYPE_DHT11
#define DHT_SENSOR_READ_DELAY_MS 2000

extern "C" void app_main()
{
    for (float temperature = -1, humidity = -1;;)
    {
        vTaskDelay(pdMS_TO_TICKS(DHT_SENSOR_READ_DELAY_MS));

        if (const esp_err_t res = dht_read_float_data(DHT_SENSOR_TYPE, DHT_GPIO, &humidity, &temperature); res == ESP_OK)
            ESP_LOGI(DHT_TAG, "Temperature = %2.f°C, humidity = %2.f%%", temperature, humidity);
        else
            ESP_LOGE(DHT_TAG, "Error reading sensor: 0x%x", res);
    }
}

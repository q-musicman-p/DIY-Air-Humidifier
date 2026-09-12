#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <nvs_flash.h>

#include "telemetry.h"
#include "dht_sensor.h"
#include "mqtt_client_interactor.h"

namespace 
{
static const char *MAIN_TAG = "MAIN_APP";
static const char *DHT_TAG = "DHT11";
#define DHT_SENSOR_READ_DELAY_MS 2000

#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASS "YOUR_PASSWORD"

static QueueHandle_t dht_queue = nullptr;
static bool is_wifi_connected = false;

struct mqqt_task_params
{
	mqtt_client_interactor *client_interactor;
	QueueHandle_t queue;
};

void wifi_event_handler (void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
			esp_wifi_connect ();
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
		{
			is_wifi_connected = false;
			ESP_LOGW (MAIN_TAG, "Wi-Fi is off. Try to reconnect...");
			esp_wifi_connect ();
		} 
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
		{
			is_wifi_connected = true;
			ESP_LOGI (MAIN_TAG, "Wi-Fi successfully connected!");
		}
}

void wifi_init_sta ()
{
	esp_netif_init ();
	esp_event_loop_create_default ();
	esp_netif_create_default_wifi_sta ();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT ();
	esp_wifi_init (&cfg);

	esp_event_handler_register (WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
	esp_event_handler_register (IP_EVENT, ip_event_t::IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

	wifi_config_t wifi_config = {};
	strcpy (reinterpret_cast<char*> (wifi_config.sta.ssid), WIFI_SSID);
	strcpy (reinterpret_cast<char*> (wifi_config.sta.password), WIFI_PASS);

	esp_wifi_set_mode (WIFI_MODE_STA);
	esp_wifi_set_config (WIFI_IF_STA, &wifi_config);
	esp_wifi_start ();
}

void dht_reader_task (void *pvParameters)
{
	auto *sensor = static_cast<dht_sensor *> (pvParameters);

	for (;;)
		{
			vTaskDelay (pdMS_TO_TICKS (DHT_SENSOR_READ_DELAY_MS));

			if (const auto data = sensor->read_data ())
				{
					ESP_LOGI (DHT_TAG, "Temperature = %d°C, humidity = %d%%", data->temperature, data->humidity);
					xQueueSend (dht_queue, &*data, 0);
				}
			else
				ESP_LOGE (DHT_TAG, "%s", data.error ().c_str ());
		}
}

void mqtt_sender_task (void *pvParameters)
{
	auto *params = static_cast<mqqt_task_params *> (pvParameters);
	telemetry received_data;

	for (;;) 
		{
			if (xQueueReceive (params->queue, &received_data, portMAX_DELAY) == pdTRUE)
				{
					if (is_wifi_connected && params->client_interactor->is_connected ())
						{
							if (params->client_interactor->publish_telemetry (received_data))
								ESP_LOGI(MAIN_TAG, "The data has successfully published to the MQTT topic.");
							else
								ESP_LOGE(MAIN_TAG, "Error while pusblishing to MQTT.");
						} 
					else
						ESP_LOGW (MAIN_TAG, "Sending is unaviable: there is not Wi-Fi network or MQTT.");
				}
		}
}
} // namespace

extern "C" void app_main ()
{
	esp_err_t ret = nvs_flash_init ();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
		{
			ESP_ERROR_CHECK (nvs_flash_erase ());
			ret = nvs_flash_init ();
		}
	ESP_ERROR_CHECK (ret);

	dht_queue = xQueueCreate (5, sizeof (telemetry));
	if (dht_queue == nullptr)
		{
			ESP_LOGE (MAIN_TAG, "Critical error: cannot create DHT event queue.");
			return;
		}

	wifi_init_sta ();

	while (!is_wifi_connected)
		vTaskDelay (pdMS_TO_TICKS (100));

	static auto sensor = dht_sensor{gpio_num_t::GPIO_NUM_18, dht_sensor_type_t::DHT_TYPE_DHT11};
	static auto client_interactor = mqtt_client_interactor{"mqtt://192.168.1.10:1883", "esp32/telemetry"};

	client_interactor.start ();
	static auto mqtt_params = mqqt_task_params{&client_interactor, dht_queue};

	xTaskCreate (dht_reader_task, "dht_reader_task", 4096, &sensor, 4, nullptr);
	xTaskCreate (mqtt_sender_task, "mqtt_sender_task", 4096, &mqtt_params, 5, nullptr);
}

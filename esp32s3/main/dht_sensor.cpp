#include "dht_sensor.h"

#include "telemetry.h"

dht_sensor::dht_sensor (gpio_num_t pin, dht_sensor_type_t sensor_type)
    : m_pin (pin), m_sensor_type (sensor_type) 
{
}

dht_sensor::~dht_sensor () = default;

std::expected<telemetry, std::string> dht_sensor::read_data () const
{
  int16_t temperature = -1;
  int16_t humidity = -1;

  const esp_err_t res = dht_read_data (m_sensor_type, m_pin, &humidity, &temperature);
  if (res !=  ESP_OK)
    {
      char buffer[63];
      snprintf (buffer, sizeof (buffer), "Error reading sensor 0x%x", res);
      return std::unexpected{std::string{buffer}};
    }
  
  return telemetry{ 
      .temperature = static_cast<int16_t> (temperature / 10), 
      .humidity = static_cast<int16_t> (humidity / 10), 
    };
}

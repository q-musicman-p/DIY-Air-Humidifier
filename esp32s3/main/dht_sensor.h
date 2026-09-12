#pragma once

#include <dht.h>
#include <expected>
#include <string>

struct telemetry;

class dht_sensor
{
  gpio_num_t m_pin;
  dht_sensor_type_t m_sensor_type;

public:
  dht_sensor (gpio_num_t pin, dht_sensor_type_t sensor_type);
  ~dht_sensor ();

  [[nodiscard]] std::expected<telemetry, std::string> read_data () const;
};

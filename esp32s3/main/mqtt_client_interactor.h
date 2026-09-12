#pragma once

#include <string>
#include "mqtt_client.h"

struct telemetry;

class mqtt_client_interactor
{
  std::string m_broker_url;
  std::string m_topic;
  esp_mqtt_client_handle_t m_client = nullptr;
  bool m_is_connected = false;

public:
  mqtt_client_interactor (std::string broker_url, std::string topic);
  ~mqtt_client_interactor ();

  void start ();
  bool is_connected () const;

  bool publish_telemetry (const telemetry &data);
};

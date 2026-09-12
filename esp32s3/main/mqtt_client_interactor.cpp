#include "mqtt_client_interactor.h"

#include "esp_log.h"
#include "telemetry.h"

namespace
{
const char *log_message_tag = "MQTT CLIENT";

void mqtt_event_handler (void *event_handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  bool *is_connected = static_cast<bool *> (event_handler_args);
  auto *event = static_cast<esp_mqtt_event_handle_t> (event_data);

  switch (static_cast<esp_mqtt_event_id_t> (event_id))
  {
    case MQTT_EVENT_CONNECTED:
      *is_connected = true;
      ESP_LOGI (log_message_tag, "Successfully connected to Mosquitto!");
      break;
    case MQTT_EVENT_DISCONNECTED:
      *is_connected = false;
      ESP_LOGW (log_message_tag, "The connection with Mosquitto was lost");
      break;
    case MQTT_EVENT_ERROR:
      ESP_LOGE (log_message_tag, "MQTT error: %d", event->error_handle->error_type);
      break;
    default:
      break;
  }
};
}  // namespace

mqtt_client_interactor::mqtt_client_interactor (std::string broker_url, std::string topic) 
  : m_broker_url (std::move(broker_url)), m_topic (std::move(topic))
{
}

mqtt_client_interactor::~mqtt_client_interactor () = default;

void mqtt_client_interactor:: start ()
{
  esp_mqtt_client_config_t mqtt_config = {};
  mqtt_config.broker.address.uri = m_broker_url.c_str();

  m_client = esp_mqtt_client_init (&mqtt_config);
  esp_mqtt_client_register_event (m_client, esp_mqtt_event_id_t::MQTT_EVENT_ANY, mqtt_event_handler, &m_is_connected);
  esp_mqtt_client_start (m_client);
}

bool mqtt_client_interactor::is_connected () const { return m_is_connected; }

bool mqtt_client_interactor::publish_telemetry (const telemetry &data)
{
  if (!m_is_connected || m_client == nullptr)
    return false;

  char json_payload[64];
  snprintf (json_payload, sizeof (json_payload), "{\"temperature\":%d,\"humidity\":%d}", data.temperature, data.humidity);

  int msg_id = esp_mqtt_client_publish (m_client, m_topic.c_str (), json_payload, 0, 1, 0);
  return msg_id != -1;
}

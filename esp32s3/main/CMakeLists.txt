idf_component_register(SRCS "mqtt_client_interactor.cpp" "dht_sensor.cpp" "esp32s3.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp_wifi nvs_flash)
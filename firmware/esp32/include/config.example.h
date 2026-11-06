#pragma once

#define WIFI_SSID       "NOME_DA_REDE"
#define WIFI_PASSWORD   "SENHA_DA_REDE"

#define MQTT_HOST       "192.168.1.100"
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""

// SOIL_RAW_DRY: leitura aproximada no solo seco.
// SOIL_RAW_WET: leitura aproximada no solo úmido.
// Em muitos sensores capacitivos, a leitura diminui quando a umidade aumenta.
#define SOIL_RAW_DRY    3000
#define SOIL_RAW_WET    1200
#define SOIL_ALERT_THRESHOLD 30

#pragma once


#define WIFI_SSID       "NOME_DA_REDE_WIFI"
#define WIFI_PASSWORD   "SENHA_DA_REDE_WIFI"

#define MQTT_HOST       "192.168.1.100"
#define MQTT_PORT       1883
#define MQTT_USERNAME   "iotuser"
#define MQTT_PASSWORD   ""
#define MQTT_CLIENT_ID  "esp32-monitoramento-01"

#define MQTT_USE_TLS   0

static const char MQTT_ROOT_CA[] = R"EOF(
-----BEGIN CERTIFICATE-----
INSIRA_AQUI_O_CERTIFICADO_CA
-----END CERTIFICATE-----
)EOF";

#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"

// GPIOs definidos no projeto.
#define PIN_ECG             34
#define PIN_DHT             4
#define PIN_SOIL            35
#define PIN_LED             25
#define PIN_BUZZER          26

// Calibração do SEN0193: substitua pelos valores medidos no seu sensor.
// Em geral, registre raw_dry com o sensor no ar/solo seco e raw_wet em
// substrato muito úmido. A orientação pode variar conforme o módulo.
#define SOIL_RAW_DRY        3000
#define SOIL_RAW_WET        1500

#define SOIL_DRY_ON_PCT     30.0f
#define SOIL_DRY_OFF_PCT    35.0f

#define DEVICE_ID           "esp32-monitoramento-01"

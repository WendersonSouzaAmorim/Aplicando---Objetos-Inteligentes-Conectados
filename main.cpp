#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

#include "config.h"

#ifndef DHTTYPE
#define DHTTYPE DHT22
#endif

// Tópicos MQTT: mantenha esta convenção sincronizada com nodered/flows.json.
static const char *TOPIC_ENV = "monitoramento/esp32/telemetry/environment";
static const char *TOPIC_ECG = "monitoramento/esp32/telemetry/ecg";
static const char *TOPIC_STATUS = "monitoramento/esp32/actuator/status";
static const char *TOPIC_COMMAND = "monitoramento/esp32/actuator/command";

static constexpr uint32_t ECG_SAMPLE_INTERVAL_MS = 40;  // 25 amostras/s
static constexpr uint8_t ECG_BATCH_SIZE = 25;           // aproximadamente 1 s
static constexpr uint32_t SOIL_INTERVAL_MS = 1000;
static constexpr uint32_t DHT_INTERVAL_MS = 2500;
static constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
static constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 5000;
static constexpr uint8_t SOIL_AVERAGE_SAMPLES = 10;
static constexpr size_t MQTT_BUFFER_BYTES = 2048;

DHT dht(PIN_DHT, DHTTYPE);
WiFiClient wifiClient;
WiFiClientSecure secureClient;
PubSubClient mqttClient;

uint16_t ecgSamples[ECG_BATCH_SIZE] = {0};
uint8_t ecgSampleCount = 0;
uint32_t ecgSequence = 0;

float temperatureC = NAN;
float humidityPct = NAN;
uint64_t dhtTimestampMs = 0;
bool dhtReadingValid = false;

int soilRaw = 0;
float soilPct = NAN;
uint64_t soilTimestampMs = 0;
bool soilReadingValid = false;
bool soilDryAlarm = false;

bool ledState = false;
bool buzzerState = false;
enum class ActuatorMode { AUTO_MODE, FORCE_ON, FORCE_OFF };
ActuatorMode actuatorMode = ActuatorMode::AUTO_MODE;

uint32_t lastEcgSampleAt = 0;
uint32_t lastSoilReadAt = 0;
uint32_t lastDhtReadAt = 0;
uint32_t lastWifiAttemptAt = 0;
uint32_t lastMqttAttemptAt = 0;

uint64_t unixTimeMs() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    if (tv.tv_sec < 1700000000) return 0; // NTP ainda não sincronizou.
    return static_cast<uint64_t>(tv.tv_sec) * 1000ULL +
           static_cast<uint64_t>(tv.tv_usec / 1000);
}

const char *modeName() {
    switch (actuatorMode) {
        case ActuatorMode::FORCE_ON: return "ON";
        case ActuatorMode::FORCE_OFF: return "OFF";
        default: return "AUTO";
    }
}

void publishActuatorStatus();

void applyActuatorOutputs() {
    digitalWrite(PIN_LED, ledState ? HIGH : LOW);
    // Para buzzer de corrente superior à capacidade do GPIO, use transistor
    // ou driver adequado. Não alimente buzzer de potência diretamente pelo pino.
    digitalWrite(PIN_BUZZER, buzzerState ? HIGH : LOW);
}

void evaluateAutomaticAlarm() {
    if (soilReadingValid) {
        if (soilPct <= SOIL_DRY_ON_PCT) soilDryAlarm = true;
        else if (soilPct >= SOIL_DRY_OFF_PCT) soilDryAlarm = false;
    }

    bool nextLed = false;
    bool nextBuzzer = false;
    if (actuatorMode == ActuatorMode::FORCE_ON) {
        nextLed = true;
        nextBuzzer = true;
    } else if (actuatorMode == ActuatorMode::AUTO_MODE) {
        nextLed = soilReadingValid && soilDryAlarm;
        nextBuzzer = soilReadingValid && soilDryAlarm;
    }

    const bool changed = (nextLed != ledState) || (nextBuzzer != buzzerState);
    ledState = nextLed;
    buzzerState = nextBuzzer;
    applyActuatorOutputs();
    if (changed && mqttClient.connected()) publishActuatorStatus();
}

float calculateSoilPercentage(int rawValue) {
    if (SOIL_RAW_DRY == SOIL_RAW_WET) return NAN;
    // Conversão orientada pelos pontos de calibração definidos em config.h.
    float value = 100.0f * (static_cast<float>(SOIL_RAW_DRY - rawValue) /
                            static_cast<float>(SOIL_RAW_DRY - SOIL_RAW_WET));
    if (value < 0.0f) value = 0.0f;
    if (value > 100.0f) value = 100.0f;
    return value;
}

bool publishDocument(const char *topic, JsonDocument &doc) {
    if (!mqttClient.connected()) return false;
    char payload[MQTT_BUFFER_BYTES];
    const size_t length = serializeJson(doc, payload, sizeof(payload));
    if (length == 0 || length >= sizeof(payload)) {
        Serial.printf("[MQTT] Payload inválido ou maior que %u bytes\n",
                      static_cast<unsigned>(sizeof(payload)));
        return false;
    }
    return mqttClient.publish(topic, reinterpret_cast<const uint8_t *>(payload), length, false);
}

void publishActuatorStatus() {
    JsonDocument doc;
    doc["device_id"] = DEVICE_ID;
    doc["timestamp_ms"] = unixTimeMs();
    doc["mode"] = modeName();
    doc["led"] = ledState;
    doc["buzzer"] = buzzerState;
    doc["soil_dry_alarm"] = soilDryAlarm;
    publishDocument(TOPIC_STATUS, doc);
}

void publishEnvironment() {
    JsonDocument doc;
    doc["device_id"] = DEVICE_ID;
    doc["timestamp_ms"] = soilTimestampMs;
    doc["soil_raw"] = soilRaw;
    if (soilReadingValid && !isnan(soilPct)) doc["soil_pct"] = soilPct;
    else doc["soil_pct"] = nullptr;
    doc["soil_calibrated"] = (SOIL_RAW_DRY != SOIL_RAW_WET);
    doc["dht_timestamp_ms"] = dhtTimestampMs;
    if (dhtReadingValid) {
        doc["temperature_c"] = temperatureC;
        doc["humidity_pct"] = humidityPct;
    } else {
        doc["temperature_c"] = nullptr;
        doc["humidity_pct"] = nullptr;
    }
    publishDocument(TOPIC_ENV, doc);
}

void publishEcgBatch() {
    if (ecgSampleCount != ECG_BATCH_SIZE) return;
    JsonDocument doc;
    doc["device_id"] = DEVICE_ID;
    doc["timestamp_ms"] = unixTimeMs();
    doc["sequence"] = ++ecgSequence;
    doc["sample_rate_hz"] = 25;
    JsonArray samples = doc["samples"].to<JsonArray>();
    for (uint8_t i = 0; i < ECG_BATCH_SIZE; ++i) samples.add(ecgSamples[i]);
    publishDocument(TOPIC_ECG, doc);
    ecgSampleCount = 0;
}

void setModeFromCommand(const char *command) {
    if (strcmp(command, "ON") == 0) actuatorMode = ActuatorMode::FORCE_ON;
    else if (strcmp(command, "OFF") == 0) actuatorMode = ActuatorMode::FORCE_OFF;
    else if (strcmp(command, "AUTO") == 0) actuatorMode = ActuatorMode::AUTO_MODE;
    else {
        Serial.printf("[MQTT] Comando desconhecido: %s\n", command);
        return;
    }
    evaluateAutomaticAlarm();
    publishActuatorStatus();
    Serial.printf("[ATUADORES] Modo alterado para %s\n", modeName());
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
    if (strcmp(topic, TOPIC_COMMAND) != 0) return;
    if (length == 0 || length > 16) return;
    char command[17] = {0};
    memcpy(command, payload, length);
    for (unsigned int i = 0; i < length; ++i) command[i] = toupper(command[i]);
    setModeFromCommand(command);
}

void configureNetworkClient() {
#if MQTT_USE_TLS
    secureClient.setCACert(MQTT_ROOT_CA);
    mqttClient.setClient(secureClient);
#else
    mqttClient.setClient(wifiClient);
#endif
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(MQTT_BUFFER_BYTES);
    mqttClient.setKeepAlive(30);
    mqttClient.setSocketTimeout(2);
}

void connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return;
    const uint32_t now = millis();
    if (now - lastWifiAttemptAt < WIFI_RETRY_INTERVAL_MS && lastWifiAttemptAt != 0) return;
    lastWifiAttemptAt = now;
    Serial.printf("[Wi-Fi] Conectando a %s...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectMqtt() {
    if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) return;
    const uint32_t now = millis();
    if (now - lastMqttAttemptAt < MQTT_RETRY_INTERVAL_MS && lastMqttAttemptAt != 0) return;
    lastMqttAttemptAt = now;

    Serial.printf("[MQTT] Conectando a %s:%d...\n", MQTT_HOST, MQTT_PORT);
    const bool connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);
    if (connected) {
        Serial.println("[MQTT] Conectado.");
        mqttClient.subscribe(TOPIC_COMMAND, 0);
        publishActuatorStatus();
    } else {
        Serial.printf("[MQTT] Falha, estado=%d\n", mqttClient.state());
    }
}

void readDhtIfDue(uint32_t now) {
    if (now - lastDhtReadAt < DHT_INTERVAL_MS) return;
    lastDhtReadAt = now;
    const float h = dht.readHumidity();
    const float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
        dhtReadingValid = false;
        Serial.println("[DHT22] Falha na leitura.");
        return;
    }
    humidityPct = h;
    temperatureC = t;
    dhtTimestampMs = unixTimeMs();
    dhtReadingValid = true;
}

void readSoilIfDue(uint32_t now) {
    if (now - lastSoilReadAt < SOIL_INTERVAL_MS) return;
    lastSoilReadAt = now;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < SOIL_AVERAGE_SAMPLES; ++i) {
        sum += analogRead(PIN_SOIL);
        delay(2);
    }
    soilRaw = static_cast<int>(sum / SOIL_AVERAGE_SAMPLES);
    soilPct = calculateSoilPercentage(soilRaw);
    soilTimestampMs = unixTimeMs();
    soilReadingValid = !isnan(soilPct);
    evaluateAutomaticAlarm();
    publishEnvironment();
}

void sampleEcgIfDue(uint32_t now) {
    if (now - lastEcgSampleAt < ECG_SAMPLE_INTERVAL_MS) return;
    lastEcgSampleAt = now;
    ecgSamples[ecgSampleCount++] = static_cast<uint16_t>(analogRead(PIN_ECG));
    if (ecgSampleCount >= ECG_BATCH_SIZE) publishEcgBatch();
}

void setup() {
    Serial.begin(115200);
    delay(200);

    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    digitalWrite(PIN_BUZZER, LOW);

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_ECG, ADC_11db);
    analogSetPinAttenuation(PIN_SOIL, ADC_11db);

    dht.begin();
    configureNetworkClient();
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
    connectWifi();

    Serial.println("Sistema de monitoramento IoT iniciado.");
    Serial.println("ATENÇÃO: ECG experimental; não é equipamento de diagnóstico médico.");
}

void loop() {
    connectWifi();
    if (WiFi.status() == WL_CONNECTED) connectMqtt();
    mqttClient.loop();

    const uint32_t now = millis();
    readDhtIfDue(now);
    readSoilIfDue(now);
    sampleEcgIfDue(now);
    delay(1);
}

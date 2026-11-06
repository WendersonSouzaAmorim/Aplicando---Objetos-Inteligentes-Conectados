#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"

// Hardware
constexpr uint8_t PIN_ECG = 34;
constexpr uint8_t PIN_DHT = 4;
constexpr uint8_t PIN_SOIL = 35;
constexpr uint8_t PIN_LED = 25;
constexpr uint8_t PIN_BUZZER = 26;
constexpr uint8_t DHT_TYPE = DHT22;

// MQTT
const char* TOPIC_TELEMETRY = "monitoramento/sensores";
const char* TOPIC_COMMANDS  = "monitoramento/comandos";
const char* TOPIC_STATUS    = "monitoramento/status";

constexpr uint32_t ECG_SAMPLE_INTERVAL_US = 40000; // 25 Hz
constexpr size_t ECG_BATCH_SIZE = 25;
constexpr uint32_t DHT_INTERVAL_MS = 2500;
constexpr uint32_t SOIL_INTERVAL_MS = 1000;
constexpr uint32_t MQTT_RETRY_MS = 5000;

WiFiClient networkClient;
PubSubClient mqtt(networkClient);
DHT dht(PIN_DHT, DHT_TYPE);

enum OperatingMode { MODE_AUTO, MODE_MANUAL };
OperatingMode operatingMode = MODE_AUTO;

bool ledState = false;
bool buzzerState = false;
float airTemperature = NAN;
float airHumidity = NAN;
int soilRaw = 0;
int soilPercent = 0;

uint16_t ecgSamples[ECG_BATCH_SIZE];
size_t ecgIndex = 0;
uint32_t lastEcgSampleUs = 0;
uint32_t lastDhtReadMs = 0;
uint32_t lastSoilReadMs = 0;
uint32_t lastMqttAttemptMs = 0;
uint32_t lastTelemetryMs = 0;
uint64_t lastEcgBatchTimestampMs = 0;

uint64_t epochMillis() {
  time_t now = time(nullptr);
  if (now < 1700000000) return 0; // NTP ainda não sincronizado
  return static_cast<uint64_t>(now) * 1000ULL;
}

void applyActuators(bool led, bool buzzer) {
  ledState = led;
  buzzerState = buzzer;
  digitalWrite(PIN_LED, ledState ? HIGH : LOW);
  digitalWrite(PIN_BUZZER, buzzerState ? HIGH : LOW);
}

void publishStatus(const char* command) {
  if (!mqtt.connected()) return;

  JsonDocument doc;
  doc["mode"] = operatingMode == MODE_AUTO ? "AUTO" : "MANUAL";
  doc["led"] = ledState ? "ON" : "OFF";
  doc["buzzer"] = buzzerState ? "ON" : "OFF";
  doc["last_command"] = command;
  doc["timestamp_ms"] = epochMillis();

  char payload[256];
  size_t length = serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(TOPIC_STATUS, payload, length);
}

void applyAutomaticRule() {
  const bool alert = soilPercent < SOIL_ALERT_THRESHOLD;
  applyActuators(alert, alert);
  publishStatus("AUTO_UPDATE");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != TOPIC_COMMANDS) return;

  String command;
  command.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    command += static_cast<char>(payload[i]);
  }
  command.trim();
  command.toUpperCase();

  if (command == "ON") {
    operatingMode = MODE_MANUAL;
    applyActuators(true, true);
    publishStatus("ON");
  } else if (command == "OFF") {
    operatingMode = MODE_MANUAL;
    applyActuators(false, false);
    publishStatus("OFF");
  } else if (command == "AUTO") {
    operatingMode = MODE_AUTO;
    applyAutomaticRule();
    publishStatus("AUTO");
  } else {
    publishStatus("INVALID_COMMAND");
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }
}

void connectMqtt() {
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastMqttAttemptMs < MQTT_RETRY_MS) return;

  lastMqttAttemptMs = millis();
  String clientId = "esp32-monitoramento-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  bool connected;
  if (strlen(MQTT_USER) > 0) {
    connected = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
  } else {
    connected = mqtt.connect(clientId.c_str());
  }

  if (connected) {
    mqtt.subscribe(TOPIC_COMMANDS, 0);
    publishStatus("CONNECTED");
  }
}

int calculateSoilPercent(int rawValue) {
  long percentage = map(rawValue, SOIL_RAW_DRY, SOIL_RAW_WET, 0, 100);
  return constrain(static_cast<int>(percentage), 0, 100);
}

void readSoilSensor() {
  long total = 0;
  for (int i = 0; i < 10; i++) {
    total += analogRead(PIN_SOIL);
    delay(2);
  }
  soilRaw = static_cast<int>(total / 10);
  soilPercent = calculateSoilPercent(soilRaw);

  if (operatingMode == MODE_AUTO) {
    applyAutomaticRule();
  }
}

void readDhtSensor() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (!isnan(humidity) && !isnan(temperature)) {
    airHumidity = humidity;
    airTemperature = temperature;
  }
}

void collectEcgSample() {
  const uint32_t nowUs = micros();
  if (static_cast<uint32_t>(nowUs - lastEcgSampleUs) < ECG_SAMPLE_INTERVAL_US) return;
  lastEcgSampleUs = nowUs;

  ecgSamples[ecgIndex++] = analogRead(PIN_ECG);
  if (ecgIndex >= ECG_BATCH_SIZE) {
    ecgIndex = 0;
    uint64_t endTimestamp = epochMillis();
    lastEcgBatchTimestampMs = endTimestamp > 0
      ? endTimestamp - (ECG_BATCH_SIZE - 1) * 40ULL
      : 0;
  }
}

void publishTelemetry() {
  if (!mqtt.connected()) return;

  JsonDocument doc;
  doc["device_id"] = "esp32-monitoramento-01";
  doc["timestamp_ms"] = epochMillis();
  doc["temperature_c"] = isnan(airTemperature) ? nullptr : airTemperature;
  doc["air_humidity_percent"] = isnan(airHumidity) ? nullptr : airHumidity;
  doc["soil_raw"] = soilRaw;
  doc["soil_humidity_percent"] = soilPercent;
  doc["ecg_sample_rate_hz"] = 25;
  doc["ecg_timestamp_ms"] = lastEcgBatchTimestampMs;

  JsonArray ecg = doc["ecg"].to<JsonArray>();
  for (size_t i = 0; i < ECG_BATCH_SIZE; i++) {
    ecg.add(ecgSamples[i]);
  }

  char payload[1200];
  size_t length = serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(TOPIC_TELEMETRY, payload, length, false);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  applyActuators(false, false);

  analogReadResolution(12);
  dht.begin();

  WiFi.setSleep(false);
  connectWiFi();

  configTime(0, 0, "pool.ntp.org", "time.google.com");

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(2048);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  connectMqtt();
  mqtt.loop();

  collectEcgSample();

  if (millis() - lastDhtReadMs >= DHT_INTERVAL_MS) {
    lastDhtReadMs = millis();
    readDhtSensor();
  }

  if (millis() - lastSoilReadMs >= SOIL_INTERVAL_MS) {
    lastSoilReadMs = millis();
    readSoilSensor();
  }

  // Publica um lote de ECG a cada segundo (25 amostras por lote).
  if (millis() - lastTelemetryMs >= 1000 && ecgIndex == 0) {
    lastTelemetryMs = millis();
    publishTelemetry();
  }

  delay(1);
}

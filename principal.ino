
#include <WiFi.h>          // Utilizo para conectar o ESP32 ao Wi-Fi.
#include <PubSubClient.h>  // Utilizo para a comunicação MQTT.
#include <DHT.h>           // Utilizo para ler o DHT22.
#include <ArduinoJson.h>   // Utilizo para montar e interpretar JSON.
#include <time.h>          // Utilizo para configurar o NTP.
#include <sys/time.h>      // Utilizo para consultar o relógio.
#include <math.h>          // Utilizo para verificar valores inválidos.

// ============================================================
// PROJETO OIC - AULA 4
// ============================================================

// Integro o ESP32 ao Wi-Fi, HiveMQ e Node-RED.
// Leio os sensores, publico a telemetria e recebo comandos MQTT.
// No Wokwi, utilizo potenciômetros para simular o ECG e o solo.


// 1. WI-FI

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";


// 2. MQTT

const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;

const char* TOPIC_SENSORES =
  "mck/oic/a4/demo-48317/monitoramento/sensores";

const char* TOPIC_COMANDOS =
  "mck/oic/a4/demo-48317/monitoramento/comandos";

const char* TOPIC_STATUS =
  "mck/oic/a4/demo-48317/monitoramento/status";


// 3. PINOS

constexpr uint8_t PIN_ECG = 34;
constexpr uint8_t PIN_SOLO = 35;
constexpr uint8_t PIN_DHT = 4;
constexpr uint8_t PIN_LED = 25;
constexpr uint8_t PIN_BUZZER = 26;

#define DHT_TYPE DHT22

DHT dht(PIN_DHT, DHT_TYPE);


// 4. OBJETOS DE COMUNICAÇÃO

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);


// 5. INTERVALOS

constexpr uint32_t ECG_INTERVAL_MS = 40;
constexpr uint32_t DHT_INTERVAL_MS = 2500;
constexpr uint32_t SOIL_INTERVAL_MS = 1000;
constexpr uint32_t WIFI_RETRY_MS = 5000;
constexpr uint32_t MQTT_RETRY_MS = 5000;

constexpr size_t ECG_BATCH_SIZE = 25;


// 6. CALIBRAÇÃO DO SOLO

constexpr int SOIL_DRY_RAW = 3500;
constexpr int SOIL_WET_RAW = 1500;

constexpr float SOIL_ALERT_LIMIT = 30.0f;


// 7. VARIÁVEIS

int ecgSamples[ECG_BATCH_SIZE];
size_t ecgIndex = 0;

float temperatureC = NAN;
float airHumidity = NAN;
float soilHumidity = 0.0f;

int soilRaw = 0;

uint64_t ecgTimestampMs = 0;
uint64_t dhtTimestampMs = 0;
uint64_t soilTimestampMs = 0;

uint32_t lastEcgMs = 0;
uint32_t lastDhtMs = 0;
uint32_t lastSoilMs = 0;

uint32_t lastWifiAttemptMs = 0;
uint32_t lastMqttAttemptMs = 0;

bool ledState = false;
bool buzzerState = false;

bool manualMode = false;
bool soilReadingValid = false;

bool wifiWasConnected = false;
bool ntpConfigured = false;
bool timeSyncLogged = false;


// 8. HORÁRIO NTP

// Converto o horário sincronizado para Unix Timestamp em milissegundos.

uint64_t unixTimeMs() {

  struct timeval tv;

  if (gettimeofday(&tv, nullptr) != 0) {
    return 0;
  }

  if (tv.tv_sec < 1700000000) {
    return 0;
  }

  return ((uint64_t)tv.tv_sec * 1000ULL) +
         ((uint64_t)tv.tv_usec / 1000ULL);
}


// Configuro o relógio do ESP32 pela internet.

void configureNTP() {

  if (!ntpConfigured) {

    configTime(
      0,
      0,
      "pool.ntp.org",
      "time.nist.gov"
    );

    ntpConfigured = true;

    Serial.println("Sincronizacao NTP iniciada.");
  }

  if (!timeSyncLogged && unixTimeMs() > 0) {

    timeSyncLogged = true;

    Serial.print("Horario NTP sincronizado: ");
    Serial.println((unsigned long long)unixTimeMs());
  }
}


// 9. ATUADORES

// Centralizo o acionamento do LED e do buzzer.

void applyOutputs(bool on) {

  ledState = on;
  buzzerState = on;

  digitalWrite(PIN_LED, on ? HIGH : LOW);
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}


// 10. LEITURA DO SOLO

// Calculo a média de dez leituras analógicas.

int readSoilAverage() {

  uint32_t total = 0;

  for (int i = 0; i < 10; i++) {

    total += analogRead(PIN_SOLO);

    delay(2);
  }

  return (int)(total / 10);
}


// Converto a leitura analógica em porcentagem.

float convertSoilToPercent(int raw) {

  float value =
    (float)(SOIL_DRY_RAW - raw) * 100.0f /
    (float)(SOIL_DRY_RAW - SOIL_WET_RAW);

  return constrain(value, 0.0f, 100.0f);
}


// 11. REGRA AUTOMÁTICA

// Aciono os atuadores quando a umidade fica abaixo de 30%.

void evaluateAutomaticRule() {

  if (manualMode || !soilReadingValid) {
    return;
  }

  bool alarm = soilHumidity < SOIL_ALERT_LIMIT;

  applyOutputs(alarm);

  Serial.print("AUTO | Umidade do solo: ");
  Serial.print(soilHumidity, 1);

  Serial.print("% | Limite: ");
  Serial.print(SOIL_ALERT_LIMIT, 1);

  Serial.print("% | LED: ");
  Serial.print(ledState ? "ON" : "OFF");

  Serial.print(" | Buzzer: ");
  Serial.println(buzzerState ? "ON" : "OFF");
}


// 12. PUBLICAÇÃO DO STATUS

// Envio ao Node-RED o estado atual do dispositivo.

void publishStatus(
  const char* result,
  const String& requestId = ""
) {

  if (!mqtt.connected()) {
    return;
  }

  StaticJsonDocument<512> doc;

  doc["result"] = result;
  doc["mode"] = manualMode ? "MANUAL" : "AUTO";

  doc["led"] = ledState ? "ON" : "OFF";
  doc["buzzer"] = buzzerState ? "ON" : "OFF";

  doc["soil_moisture_pct"] = soilHumidity;
  doc["timestamp_ms"] = unixTimeMs();

  if (requestId.length() > 0) {
    doc["request_id"] = requestId;
  }

  char payload[512];

  size_t n = serializeJson(
    doc,
    payload,
    sizeof(payload)
  );

  if (n > 0 && n < sizeof(payload)) {

    mqtt.publish(
      TOPIC_STATUS,
      (const uint8_t*)payload,
      (unsigned int)n,
      false
    );
  }
}


// 13. RECEBIMENTO DOS COMANDOS

// Interpreto os comandos recebidos pelo MQTT.

void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length
) {

  String message;
  message.reserve(length + 1);

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  String command;
  String requestId;

  StaticJsonDocument<512> doc;

  DeserializationError error =
    deserializeJson(doc, message);

  if (!error && doc.is<JsonObject>()) {

    if (doc["command"].is<const char*>()) {
      command = doc["command"].as<String>();

    } else if (doc["cmd"].is<const char*>()) {
      command = doc["cmd"].as<String>();
    }

    if (doc["request_id"].is<const char*>()) {
      requestId = doc["request_id"].as<String>();
    }

  } else {

    command = message;
  }

  command.trim();
  command.toUpperCase();

  Serial.print("Comando recebido: ");
  Serial.println(command);

  if (command == "ON") {

    manualMode = true;
    applyOutputs(true);

    publishStatus("COMMAND_APPLIED", requestId);

    Serial.println("Modo manual: atuadores ligados.");

  }

  else if (command == "OFF") {

    manualMode = true;
    applyOutputs(false);

    publishStatus("COMMAND_APPLIED", requestId);

    Serial.println("Modo manual: atuadores desligados.");

  }

  else if (command == "AUTO") {

    manualMode = false;

    evaluateAutomaticRule();

    publishStatus("AUTO_ENABLED", requestId);

    Serial.println("Modo automatico ativado.");

  }

  else {

    Serial.println("Comando MQTT invalido.");

    publishStatus("INVALID_COMMAND", requestId);
  }
}


// 14. RECONEXÃO WI-FI

// Tento recuperar o Wi-Fi sem reiniciar o ESP32.

void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  uint32_t now = millis();

  if (now - lastWifiAttemptMs < WIFI_RETRY_MS) {
    return;
  }

  lastWifiAttemptMs = now;

  Serial.println("Wi-Fi desconectado; tentando reconectar...");

  WiFi.reconnect();
}


// 15. RECONEXÃO MQTT

// Conecto ao HiveMQ e assino o tópico de comandos.

void connectMQTT() {

  if (
    WiFi.status() != WL_CONNECTED ||
    mqtt.connected()
  ) {
    return;
  }

  uint32_t now = millis();

  if (now - lastMqttAttemptMs < MQTT_RETRY_MS) {
    return;
  }

  lastMqttAttemptMs = now;

  uint64_t chipId = ESP.getEfuseMac();

  char clientId[48];

  snprintf(
    clientId,
    sizeof(clientId),
    "wokwi-oic-a4-%04X",
    (uint16_t)(chipId & 0xFFFF)
  );

  Serial.print("Conectando ao HiveMQ... ");

  if (mqtt.connect(clientId)) {

    Serial.println("conectado!");

    mqtt.subscribe(TOPIC_COMANDOS, 0);

    publishStatus("DEVICE_ONLINE");

  } else {

    Serial.print("Falha MQTT. Estado: ");
    Serial.println(mqtt.state());
  }
}


// 16. LEITURA DOS SENSORES

// Organizo as leituras respeitando o intervalo de cada sensor.

void readSensors() {

  uint32_t now = millis();


  // ECG

  if (
    ecgIndex < ECG_BATCH_SIZE &&
    now - lastEcgMs >= ECG_INTERVAL_MS
  ) {

    lastEcgMs = now;

    ecgSamples[ecgIndex] = analogRead(PIN_ECG);

    ecgIndex++;

    ecgTimestampMs = unixTimeMs();
  }


  // DHT22

  // Faço uma nova leitura se o intervalo venceu
  // ou se ainda preciso de um timestamp válido.

  if (
    now - lastDhtMs >= DHT_INTERVAL_MS ||
    (dhtTimestampMs == 0 && unixTimeMs() > 0)
  ) {

    lastDhtMs = now;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {

      uint64_t timestamp = unixTimeMs();

      airHumidity = h;
      temperatureC = t;

      // Só registro o horário quando o NTP já está sincronizado.

      if (timestamp > 0) {
        dhtTimestampMs = timestamp;
      }

    } else {

      Serial.println("Falha na leitura do DHT22.");
    }
  }


  // SENSOR DE SOLO

  if (now - lastSoilMs >= SOIL_INTERVAL_MS) {

    lastSoilMs = now;

    soilRaw = readSoilAverage();

    soilHumidity = convertSoilToPercent(soilRaw);

    soilTimestampMs = unixTimeMs();

    soilReadingValid = true;

    evaluateAutomaticRule();
  }
}


// 17. PUBLICAÇÃO DA TELEMETRIA

// Reúno os dados dos sensores e dos atuadores em JSON.

void publishTelemetry() {

  if (
    !mqtt.connected() ||
    ecgIndex < ECG_BATCH_SIZE
  ) {
    return;
  }

  StaticJsonDocument<2048> doc;

  doc["device"] = "esp32-oic-01";
  doc["timestamp_ms"] = unixTimeMs();

  doc["ecg_sample_rate_hz"] = 25;
  doc["ecg_timestamp_ms"] = ecgTimestampMs;

  JsonArray ecg =
    doc.createNestedArray("ecg_samples");

  for (size_t i = 0; i < ECG_BATCH_SIZE; i++) {
    ecg.add(ecgSamples[i]);
  }


  // DHT22

  JsonObject dhtData =
    doc.createNestedObject("dht22");

  if (!isnan(temperatureC)) {
    dhtData["temperature_c"] = temperatureC;
  }

  if (!isnan(airHumidity)) {
    dhtData["air_humidity_pct"] = airHumidity;
  }

  dhtData["timestamp_ms"] = dhtTimestampMs;


  // SENSOR DE SOLO

  JsonObject soilData =
    doc.createNestedObject("sen0193");

  soilData["raw"] = soilRaw;
  soilData["moisture_pct"] = soilHumidity;
  soilData["timestamp_ms"] = soilTimestampMs;


  // ATUADORES

  JsonObject actuators =
    doc.createNestedObject("actuators");

  actuators["led"] = ledState ? "ON" : "OFF";
  actuators["buzzer"] = buzzerState ? "ON" : "OFF";

  doc["control_mode"] =
    manualMode ? "MANUAL" : "AUTO";


  // CONVERSÃO PARA JSON

  char payload[2048];

  size_t requiredSize = measureJson(doc);

  if (requiredSize >= sizeof(payload)) {

    Serial.println("Erro: payload excedeu o buffer.");

    return;
  }

  size_t n = serializeJson(
    doc,
    payload,
    sizeof(payload)
  );

  if (n == 0 || n >= sizeof(payload)) {

    Serial.println("Erro ao serializar telemetria.");

    return;
  }


  // PUBLICAÇÃO MQTT

  bool ok = mqtt.publish(
    TOPIC_SENSORES,
    (const uint8_t*)payload,
    (unsigned int)n,
    false
  );

  if (ok) {

    Serial.println("Telemetria publicada.");

    ecgIndex = 0;

  } else {

    Serial.println("Falha ao publicar telemetria.");
  }
}


// 18. CONFIGURAÇÃO INICIAL

// Configuro os componentes e inicio as conexões.

void setup() {

  Serial.begin(115200);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  applyOutputs(false);

  analogReadResolution(12);

  dht.begin();

  delay(2000);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  mqtt.setBufferSize(2048);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(5);

  Serial.println();

  Serial.println("Projeto OIC Aula 4 iniciado.");

  Serial.println("GPIO34 = ECG simulado");
  Serial.println("GPIO35 = Solo simulado");
  Serial.println("GPIO4 = DHT22");
  Serial.println("GPIO25 = LED");
  Serial.println("GPIO26 = Buzzer");

  Serial.println("Broker: broker.hivemq.com:1883");

  Serial.print("Topico de telemetria: ");
  Serial.println(TOPIC_SENSORES);

  WiFi.mode(WIFI_STA);

  WiFi.setAutoReconnect(true);

  Serial.print("Conectando ao Wi-Fi ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);

  lastWifiAttemptMs = millis();

  lastEcgMs = millis();
  lastDhtMs = millis();
  lastSoilMs = millis();
}


// 19. LOOP PRINCIPAL

// Mantenho as conexões, leio os sensores e publico os dados.

void loop() {

  bool wifiConnected =
    WiFi.status() == WL_CONNECTED;


  if (wifiConnected && !wifiWasConnected) {

    Serial.println("Wi-Fi conectado!");

    Serial.print("Endereco IP: ");
    Serial.println(WiFi.localIP());

    configureNTP();
  }


  if (!wifiConnected && wifiWasConnected) {

    Serial.println("Conexao Wi-Fi perdida.");
  }

  wifiWasConnected = wifiConnected;

  connectWiFi();


  if (WiFi.status() == WL_CONNECTED) {

    configureNTP();

    connectMQTT();

    if (mqtt.connected()) {
      mqtt.loop();
    }
  }


  readSensors();

  publishTelemetry();

  delay(1);
}

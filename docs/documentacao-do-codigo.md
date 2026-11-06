# Documentação do software e do código

## 1. Visão geral

O software é dividido em firmware embarcado no ESP32 e fluxo de integração no Node-RED. O firmware realiza aquisição dos sensores, mantém a conexão Wi-Fi, publica telemetria por MQTT, recebe comandos e controla LED e buzzer. O broker Mosquitto encaminha mensagens entre o ESP32 e o Node-RED.

## 2. Firmware (`firmware/esp32/src/main.cpp`)

O firmware foi desenvolvido em C++ para Arduino-ESP32 e utiliza as bibliotecas WiFi, PubSubClient, DHT e ArduinoJson.

- `connectWiFi()`: conecta o ESP32 à rede sem fio configurada.
- `connectMqtt()`: estabelece a sessão MQTT, utilizando credenciais quando informadas, e assina o tópico de comandos.
- `mqttCallback()`: interpreta `ON`, `OFF` e `AUTO` recebidos pelo tópico de comandos.
- `applyActuators()`: atualiza as saídas digitais do LED e do buzzer.
- `applyAutomaticRule()`: compara a umidade do solo com o limite configurado e atualiza os atuadores.
- `readDhtSensor()`: lê temperatura e umidade relativa do ar.
- `readSoilSensor()`: realiza dez leituras analógicas do sensor capacitivo e calcula a média.
- `collectEcgSample()`: coleta o sinal analógico do AD8232 a 25 Hz e organiza 25 amostras por lote.
- `publishTelemetry()`: serializa as informações em JSON e publica no tópico de telemetria.
- `publishStatus()`: publica modo de operação, estado dos atuadores e último comando.

O relógio é sincronizado por NTP. O campo `timestamp_ms` representa o horário Unix em milissegundos quando há sincronização. O firmware mantém a aquisição mesmo quando a conexão MQTT está indisponível, mas a publicação depende da reconexão.

## 3. Configuração (`config.h`)

O arquivo `config.example.h` deve ser copiado para `config.h`. Nele são definidos SSID e senha Wi-Fi, endereço e porta do broker, credenciais MQTT e valores de calibração do sensor SEN0193.

A calibração deve ser realizada medindo o valor ADC no sensor em condição seca e em condição úmida. Esses valores são configurados em `SOIL_RAW_DRY` e `SOIL_RAW_WET`. O limite que aciona LED e buzzer no modo automático é definido por `SOIL_ALERT_THRESHOLD`.

## 4. Fluxo Node-RED (`nodered/flows.json`)

O fluxo contém dois caminhos de entrada MQTT: um para `monitoramento/sensores` e outro para `monitoramento/status`. Cada caminho interpreta a mensagem JSON e a disponibiliza no painel Debug do Node-RED. Três nós Inject publicam os comandos `ON`, `OFF` e `AUTO` em `monitoramento/comandos`.

Para importar, abra o editor Node-RED, selecione Import, escolha o arquivo `flows.json` e clique em Deploy. O broker configurado no fluxo utiliza o hostname `mosquitto`, válido na rede criada pelo Docker Compose.

## 5. Compilação e gravação

Abra a pasta `firmware/esp32` no VS Code com PlatformIO. Depois de criar `include/config.h`, selecione o ambiente `esp32dev`, compile e carregue o firmware na placa. O monitor serial utiliza 115200 baud.

## 6. Formato de telemetria

O tópico `monitoramento/sensores` recebe um objeto JSON com `device_id`, `timestamp_ms`, `temperature_c`, `air_humidity_percent`, `soil_raw`, `soil_humidity_percent`, `ecg_sample_rate_hz`, `ecg_timestamp_ms` e `ecg` (vetor com 25 amostras ADC).

O tópico `monitoramento/status` recebe um objeto JSON com `mode`, `led`, `buzzer`, `last_command` e `timestamp_ms`.

## 7. Limitações de protocolo

A implementação utiliza MQTT QoS 0, que não confirma a entrega da mensagem. O mecanismo de reconexão restabelece a sessão quando Wi-Fi ou broker ficam indisponíveis. Para operação exposta à internet, configure autenticação, ACL, TLS e proteção de rede; a configuração anônima fornecida é somente para laboratório local.

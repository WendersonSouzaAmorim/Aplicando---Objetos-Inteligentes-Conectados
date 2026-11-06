# Instalação e reprodução

1. Instale VS Code e a extensão PlatformIO.
2. Instale Docker Engine e Docker Compose.
3. Clone o repositório.
4. Copie `firmware/esp32/include/config.example.h` para `firmware/esp32/include/config.h`.
5. Configure SSID, senha Wi-Fi, IP do broker e calibração do SEN0193.
6. Na raiz, execute `docker compose up -d`.
7. Abra `http://localhost:1880`, importe `nodered/flows.json` e faça Deploy.
8. Compile e carregue o firmware na placa ESP32.
9. Abra o Debug do Node-RED para acompanhar telemetria e status.
10. Utilize os nós Inject `ON`, `OFF` e `AUTO` para enviar comandos.

O IP informado em `MQTT_HOST` deve ser o endereço da máquina na rede local. O nome `mosquitto` é resolvido apenas entre contêineres Docker e não pelo ESP32.

Para encerrar os serviços, execute `docker compose down`.

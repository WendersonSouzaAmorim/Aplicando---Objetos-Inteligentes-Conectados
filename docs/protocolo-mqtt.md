# Interfaces e protocolo MQTT

## Transporte

O ESP32 conecta-se à rede por Wi-Fi e utiliza a pilha TCP/IP. O MQTT opera sobre TCP, por padrão na porta 1883. O Node-RED e o Mosquitto podem ser executados na mesma rede Docker; o ESP32 acessa o broker pelo endereço IP da máquina hospedeira.

## Tópicos

- `monitoramento/sensores`: ESP32 publica telemetria; Node-RED assina.
- `monitoramento/comandos`: Node-RED publica comandos; ESP32 assina.
- `monitoramento/status`: ESP32 publica estado e confirmação; Node-RED assina.

## Comandos

O payload é texto simples: `ON`, `OFF` ou `AUTO`. `ON` liga LED e buzzer; `OFF` desliga ambos; `AUTO` seleciona o modo automático, que compara o percentual de umidade do solo com `SOIL_ALERT_THRESHOLD`.

## Qualidade de serviço

O firmware utiliza QoS 0. Não há confirmação de entrega em nível MQTT. As mensagens não são retidas. O cliente tenta restabelecer a conexão quando ocorre uma interrupção.

## Segurança

A configuração de laboratório usa Mosquitto sem autenticação para simplificar testes locais. Não disponibilize esse broker diretamente na internet. Para acesso externo, habilite autenticação, ACL, TLS e firewall ou VPN.

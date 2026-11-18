# Sistema IoT de Monitoramento Ambiental e ECG com ESP32, MQTT e Node-RED

## Apresentação do projeto

Este projeto foi desenvolvido na disciplina Objetos Inteligentes Conectados, do curso de Análise e Desenvolvimento de Sistemas da Universidade Presbiteriana Mackenzie. O objetivo é desenvolver um sistema IoT utilizando o ESP32 para realizar o monitoramento de sinais e variáveis ambientais, transmitir os dados pela internet e permitir o acionamento remoto de dispositivos.

A solução utiliza comunicação Wi-Fi e o protocolo MQTT para integrar o ESP32 ao broker HiveMQ e à plataforma Node-RED. O sistema monitora um sinal analógico que representa o ECG, temperatura, umidade do ar e umidade do solo. Também permite controlar remotamente um LED e um buzzer, além de executar regras automáticas de acionamento conforme os valores de umidade do solo.

O projeto utiliza o ambiente Wokwi para desenvolver e testar a integração entre os componentes, o firmware, o broker MQTT e o Node-RED. Na simulação, potenciômetros representam os sinais analógicos dos módulos AD8232 e SEN0193.

## Arquitetura e funcionamento

O ESP32 realiza a leitura dos sensores e organiza os dados em mensagens JSON. Essas informações são transmitidas por Wi-Fi ao broker MQTT HiveMQ, que distribui as mensagens para os clientes inscritos nos respectivos tópicos. O Node-RED recebe os dados, permitindo acompanhar as informações de monitoramento e visualizar o funcionamento do sistema.

A comunicação também ocorre no sentido inverso. Pelo Node-RED, é possível enviar comandos MQTT ao ESP32 para ligar ou desligar o LED e o buzzer. O firmware interpreta os comandos recebidos e atualiza o estado dos atuadores. Além do controle manual, existe o modo automático, no qual o sistema aciona os dispositivos quando a umidade do solo fica abaixo do limite configurado de 30%.

## Componentes e conexões

O projeto utiliza a placa ESP32-DevKit, o módulo AD8232 para aquisição do sinal de ECG, o sensor DHT22 para medição da temperatura e umidade do ar, o sensor de umidade do solo SEN0193, um LED, um buzzer, resistores e uma protoboard.

Na simulação, os potenciômetros substituem as entradas analógicas dos módulos AD8232 e SEN0193, permitindo variar os valores recebidos pelo ESP32. Essa substituição possibilita testar o processamento dos dados e a comunicação MQTT no ambiente virtual.

As conexões utilizadas são: GPIO 34 para o sinal analógico do ECG, GPIO 35 para a leitura da umidade do solo, GPIO 4 para o sinal de dados do DHT22, GPIO 25 para o LED e GPIO 26 para o buzzer.

## Comunicação MQTT

A comunicação utiliza o broker público HiveMQ, no endereço `broker.hivemq.com`, pela porta `1883`, sem autenticação, para os testes realizados no projeto.

Foram definidos três tópicos MQTT para organizar a troca de mensagens:

Telemetria: `mck/oic/a4/demo-48317/monitoramento/sensores`

Comandos: `mck/oic/a4/demo-48317/monitoramento/comandos`

Status: `mck/oic/a4/demo-48317/monitoramento/status`

O tópico de telemetria recebe os dados coletados pelo ESP32. O tópico de comandos recebe as solicitações enviadas pelo Node-RED. O tópico de status permite acompanhar informações relacionadas ao funcionamento do dispositivo.

As mensagens são estruturadas em JSON, contendo os valores dos sensores, registros de tempo, identificação do dispositivo e estado dos atuadores.

## Aquisição e processamento dos dados

O firmware realiza a aquisição do sinal analógico de ECG com frequência de 25 Hz, coletando uma amostra a cada 40 milissegundos. As amostras são agrupadas em lotes de 25 registros antes da publicação MQTT.

A leitura do DHT22 ocorre em intervalos de aproximadamente 2.500 milissegundos, enquanto a leitura da umidade do solo ocorre a cada 1.000 milissegundos, utilizando a média de dez leituras analógicas para reduzir oscilações.

O firmware também utiliza sincronização de horário por NTP para registrar os horários das medições em milissegundos. Dessa forma, cada conjunto de dados pode ser associado ao momento em que foi adquirido.

## Controle dos atuadores

O sistema permite três possibilidades de operação: acionamento manual, desligamento manual e controle automático.

No modo manual, os comandos ON e OFF enviados pelo Node-RED controlam o LED e o buzzer. No modo automático, o ESP32 compara a umidade do solo com o limite de 30%. Quando o valor fica abaixo desse limite, os atuadores são ligados. Quando a umidade atinge ou ultrapassa 30%, os dispositivos são desligados.

## Estrutura do repositório

O arquivo `README.md` apresenta a documentação geral do projeto, incluindo sua arquitetura, funcionamento, componentes e procedimentos de reprodução.

O arquivo `sketch.ino` contém o firmware completo executado no ESP32. Nele estão implementadas a conexão Wi-Fi, a comunicação MQTT, a leitura e o processamento dos sensores, a publicação das mensagens JSON, o recebimento de comandos remotos e o controle manual e automático dos atuadores.

O arquivo `diagram.json` contém a representação das conexões e dos componentes utilizados no circuito simulado no Wokwi.

O arquivo `flows-node-red.json` contém o fluxo desenvolvido no Node-RED, responsável por receber as mensagens MQTT, processar a telemetria e enviar comandos ao ESP32.

O arquivo `libraries.txt` apresenta as bibliotecas utilizadas no desenvolvimento do firmware.

O arquivo `wokwi-project.txt` reúne as informações de identificação e acesso ao projeto no ambiente Wokwi.

## Reprodução do projeto

Para reproduzir o projeto, é necessário acessar o circuito no Wokwi, carregar o firmware disponível em `sketch.ino` e configurar a comunicação Wi-Fi utilizando a rede `Wokwi-GUEST`, sem senha, no canal 6.

Também é necessário importar o fluxo `flows-node-red.json` no Node-RED e configurar o cliente MQTT para utilizar o broker `broker.hivemq.com`, na porta `1883`. Após iniciar a simulação e o fluxo, o ESP32 estabelece a conexão com o broker e começa a publicar as informações dos sensores.

A partir do Node-RED, é possível acompanhar a telemetria e enviar comandos para controlar os atuadores. Os testes permitem verificar a transmissão dos dados, o recebimento dos comandos e o funcionamento da lógica automática de acionamento.

## Resultados dos testes

Os testes realizados demonstraram a comunicação entre o ESP32, o broker HiveMQ e o Node-RED, tanto para a transmissão de telemetria quanto para o envio de comandos de controle.

O acionamento remoto do LED e do buzzer foi validado por meio dos comandos ON e OFF. Também foi verificado o funcionamento do modo automático, utilizando o limite de 30% de umidade do solo.

Nos testes de acionamento, foram obtidos tempos médios aproximados de 1,00 segundo para ligar os atuadores e 1,25 segundo para desligá-los. Esses valores representam o tempo de resposta do sistema de comunicação e acionamento, desde o envio do comando no Node-RED até a alteração observada nos dispositivos, considerando que as medições foram registradas com resolução de segundos inteiros.

## Vídeo de apresentação

O vídeo apresenta o desenvolvimento e o funcionamento do projeto, incluindo a arquitetura utilizada, o firmware, a comunicação MQTT, a visualização da telemetria no Node-RED e os testes de acionamento dos dispositivos.

Link do vídeo: IREI COLOCAR O LINK AKI

## Referências

ESPRESSIF SYSTEMS. ESP32 Series Datasheet. Disponível em: https://www.espressif.com/. Acesso em: 18 nov. 2026.

HIVEMQ. HiveMQ MQTT Broker. Disponível em: https://www.hivemq.com/. Acesso em: 18 nov. 2026.

MQTT. MQTT: The Standard for IoT Messaging. Disponível em: https://mqtt.org/. Acesso em: 18 nov. 2026.

NODE-RED. Node-RED Documentation. Disponível em: https://nodered.org/docs/. Acesso em: 18 nov. 2026.

WOKWI. Wokwi Simulator Documentation. Disponível em: https://docs.wokwi.com/. Acesso em: 18 nov. 2026.

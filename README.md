Atividade Aplicando da disciplina Objetos Inteligentes Conectados


Sistema IoT de baixo custo para monitoramento de sinais vitais e condições ambientais utilizando ESP32, sensores AD8232, DHT22 e SEN0193. Integra comunicação TCP/IP via Wi-Fi e protocolo MQTT com Node-RED para visualização de dados e controle remoto de atuadores.


O projeto consiste em um sistema de monitoramento IoT de baixo custo, desenvolvido com a plataforma ESP32, para aquisição de sinais de ECG e de variáveis ambientais, como temperatura, umidade do ar e umidade do solo e, os dados são coletados pelos sensores, transmitidos pela rede Wi-Fi utilizando o protocolo MQTT e disponibilizados em uma interface Node-RED para visualização e acompanhamento remoto fora que, o sistema também permite o envio de comandos pela internet para controlar um LED e um buzzer e, para reproduzir o projeto, é necessário montar o circuito conforme as conexões documentadas, configurar as credenciais de Wi-Fi e do broker MQTT, carregar o firmware no ESP32 e importar o fluxo disponibilizado no Node-RED.

-------------------------------------------------------------------------------------------
Sobre o hardware

O projeto utiliza a plataforma de desenvolvimento ESP32-DevKitC, equipada com o módulo ESP32-WROOM-32, responsável pelo processamento das informações, aquisição dos dados dos sensores e comunicação sem fio por Wi-Fi. Para o monitoramento das variáveis, são utilizados três sensores: o AD8232, destinado à aquisição de sinais eletrocardiográficos (ECG), conectado ao GPIO 34; o DHT22, responsável pela medição da temperatura e da umidade relativa do ar, conectado ao GPIO 4; e o SEN0193, sensor capacitivo utilizado para medir a umidade do solo, conectado ao GPIO 35.

Como dispositivos de atuação, o sistema utiliza um LED, conectado ao GPIO 25 por meio de um resistor limitador de corrente de 220 a 330 Ω, e um buzzer ativo, conectado ao GPIO 26. O buzzer pode utilizar um transistor ou circuito driver para garantir o acionamento adequado, respeitando os limites elétricos dos pinos do ESP32. Os componentes são interligados conforme o esquema de montagem do projeto, observando a alimentação adequada e a compatibilidade dos sinais elétricos com a lógica de 3,3 V da plataforma.

A montagem utiliza uma protoboard, fios jumper e cabos de conexão para integrar os componentes eletrônicos. O conjunto permite realizar a aquisição dos sinais dos sensores, processar as informações no microcontrolador e controlar os dispositivos de sinalização. A alimentação e as conexões elétricas são organizadas de acordo com as especificações de cada componente.

Os materiais utilizados incluem uma placa ESP32-DevKitC com módulo ESP32-WROOM-32, um módulo AD8232, um sensor DHT22, um sensor capacitivo de umidade do solo SEN0193, um LED, um buzzer ativo, um resistor de 220 a 330 Ω, uma protoboard, fios jumper e cabos de conexão. Para o acionamento do buzzer, também pode ser empregado um transistor ou circuito driver, conforme sua especificação elétrica.

As conexões são realizadas da seguinte maneira: a saída analógica do AD8232 é ligada ao GPIO 34 do ESP32; o pino de dados do DHT22 é conectado ao GPIO 4; a saída analógica do SEN0193 é conectada ao GPIO 35; o LED é controlado pelo GPIO 25, utilizando o resistor limitador de corrente; e o buzzer é acionado pelo GPIO 26. Todas as conexões respeitam os limites elétricos e de alimentação dos componentes.

-------------------------------------------------------------------------------------------
Sobre documentação das interfaces, protocolos e módulos de comunicação

A comunicação entre os componentes do projeto é realizada por meio da plataforma ESP32, que utiliza conexão Wi-Fi para estabelecer comunicação com a rede local ou com a internet, utilizando a pilha de protocolos TCP/IP. Essa conectividade permite transmitir os dados coletados pelos sensores e receber comandos remotamente, integrando o hardware à infraestrutura de monitoramento.

O protocolo MQTT (Message Queuing Telemetry Transport) é utilizado como mecanismo de comunicação entre o ESP32 e o sistema de monitoramento. Baseado no modelo de publicação e assinatura (Publish/Subscribe), o MQTT utiliza um broker responsável por receber, organizar e distribuir as mensagens entre os dispositivos conectados. O ESP32 atua como cliente MQTT, publicando informações dos sensores e assinando os tópicos destinados ao recebimento de comandos. O Node-RED também atua como cliente MQTT, recebendo os dados de telemetria e disponibilizando recursos para o envio de instruções aos atuadores.

A comunicação é organizada por tópicos MQTT, que identificam o conteúdo e a finalidade das mensagens. O tópico `monitoramento/sensores` é utilizado para transmitir as informações de temperatura, umidade do ar, umidade do solo e sinais de ECG. O tópico `monitoramento/comandos` recebe instruções destinadas ao controle dos atuadores, enquanto o tópico `monitoramento/status` permite transmitir informações sobre o estado de funcionamento do dispositivo e a confirmação dos comandos executados.

Os dados são transmitidos em formato JSON (JavaScript Object Notation), permitindo organizar as informações em estruturas padronizadas e facilitar sua interpretação pelo Node-RED. As mensagens de telemetria incluem os valores obtidos pelos sensores e seus respectivos registros temporais. Os comandos de atuação utilizam os estados "ON", "OFF" e "AUTO", correspondentes ao acionamento, desligamento e funcionamento automático do LED e do buzzer.

O Node-RED é utilizado como plataforma de integração e visualização, permitindo estabelecer conexões com o broker MQTT por meio de nós de publicação e assinatura. Os dados recebidos são processados e apresentados em um painel de monitoramento, que disponibiliza informações dos sensores e permite controlar remotamente os dispositivos de atuação. A interface também possibilita acompanhar as mensagens e o estado de comunicação do sistema.

A integração entre ESP32, broker MQTT e Node-RED utiliza comunicação bidirecional: os dados dos sensores são enviados pelo microcontrolador ao broker e encaminhados ao painel de monitoramento; em sentido contrário, os comandos enviados pela interface são publicados no broker, recebidos pelo ESP32 e convertidos em ações nos pinos GPIO correspondentes.

Para a comunicação MQTT, o projeto utiliza QoS 0 (*Quality of Service 0*), no qual as mensagens são transmitidas sem confirmação de entrega pelo protocolo. O firmware também implementa procedimentos de reconexão para restabelecer a comunicação em caso de interrupção da conexão Wi-Fi ou indisponibilidade do broker. Dessa forma, a arquitetura integra aquisição de dados, transmissão pela internet, monitoramento remoto e controle de atuadores em uma estrutura baseada em TCP/IP e MQTT.


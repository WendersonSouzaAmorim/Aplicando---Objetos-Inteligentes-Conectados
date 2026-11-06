# Hardware e interfaces elétricas

O protótipo utiliza ESP32-DevKitC com ESP32-WROOM-32, AD8232, DHT22, sensor capacitivo SEN0193, LED, buzzer ativo, resistor de 220–330 Ω, protoboard e jumpers.

Conexões definidas no firmware:
- AD8232 OUT: GPIO 34 (entrada ADC).
- DHT22 DATA: GPIO 4.
- SEN0193 OUT: GPIO 35 (entrada ADC).
- LED: GPIO 25, com resistor em série.
- Buzzer ativo: GPIO 26; use transistor/driver quando necessário.

GPIO 34 e GPIO 35 são entradas. Não conecte sinais acima dos limites do ESP32 (lógica de 3,3 V). Confirme a pinagem e a tensão de alimentação dos módulos específicos antes de energizar o circuito. A saída do buzzer não deve exceder a corrente permitida pelo GPIO.

O AD8232 destina-se à aquisição experimental de ECG e não constitui equipamento médico ou instrumento de diagnóstico. A conexão de eletrodos a uma pessoa exige cuidados de isolamento elétrico apropriados; não realize medições com o corpo conectado a circuitos ligados à rede elétrica ou a equipamentos aterrados sem isolamento adequado.

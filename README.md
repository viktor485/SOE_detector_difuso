<<<<<<< HEAD
# SOE_detector_difuso
Projeto de Sistemas Operacionais Embarcados, cuja finalidade é implementar um detector integrado com Raspberry pi 3 e camera, para verificar desvios de conduta em direção defensiva de veículos automotores.

## 🛠 Hardware
- Raspberry Pi 2/3
- Câmera (CSI ou USB)
- Buzzer Passivo KY-006 (Saída PWM - GPIO 12)
- LEDs de Redundância (GPIOs 22 a 27)

## 📌 Pinagem (GPIO)
| Componente | Pino BCM | Função |
|------------|----------|--------|
| Buzzer     | GPIO 12  | Alerta Sonoro (PWM) |
| LED Red    | GPIO 22  | Alerta Crítico/sono |
| LED Green  | GPIO 23  | Sistema OK |
| LED Blue   | GPIO 24  | Piscada não intermitente |
| LED Yellow | GPIO 25  | Virar cabeça |
| LED Blue   | GPIO 26  | Falar ao telefone |
| LED Yellow | GPIO 27  | Sistema off/falha |
=======
# Monitor de Sonolência com Raspberry Pi 3

Projeto para disciplina de Sistemas Operacionais Embarcados.

## Objetivo
Detectar sinais de sonolência de motoristas usando câmera, visão computacional e alertas físicos com LED e buzzer.

## Hardware
- Raspberry Pi 3
- Câmera
- LED
- Buzzer
- Protoboard

## Software
- C++
- OpenCV
- lgpio

## Estado atual
- Detecção facial
- EAR para olhos
- MAR para bocejo
- Acionamento de LED e buzzer
>>>>>>> 2dcb600 (feat:versao inicial do detectoir de atencao)

# SOE_detector_difuso
Projeto de Sistemas Operacionais Embarcados, cuja finalidade é implementar um detector integrado com Raspberry pi 3 e camera, para verificar desvios de conduta em direção defensiva de veículos automotores.

## 🛠 Hardware
- Raspberry Pi 2/3
- Câmera (CSI ou USB)
- Buzzer Passivo KY-006 (Saída PWM - GPIO 18)
- LEDs de Redundância (GPIOs 17,22 e 27)

## 📌 Pinagem (GPIO)
| Componente | Pino BCM | Função |
|------------|----------|--------|
| Buzzer     | GPIO 18  | Alerta Sonoro (PWM) |
| LED Red    | GPIO 17  | Alerta Crítico/sono (olhos fechados e cabeça baixa) |
| LED Yellow | GPIO 22  | Bocejo |
| LED White  | GPIO 27  | Sistema off/falha |

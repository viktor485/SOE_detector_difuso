#include <lgpio.h>
#include <iostream>
#include <unistd.h>

const int PIN_LED = 17;     // GPIO 17
const int PIN_BUZZER = 18;  // GPIO 18

int main() {
    int h_gpio = lgGpiochipOpen(0);
    if (h_gpio < 0) {
        std::cerr << "Erro ao abrir gpiochip: " << h_gpio << std::endl;
        return 1;
    }

    int ret = lgGpioClaimOutput(h_gpio, 0, PIN_LED, 0);
    if (ret < 0) {
        std::cerr << "Erro ao configurar LED: " << ret << std::endl;
        lgGpiochipClose(h_gpio);
        return 1;
    }

    ret = lgGpioClaimOutput(h_gpio, 0, PIN_BUZZER, 0);
    if (ret < 0) {
        std::cerr << "Erro ao configurar buzzer: " << ret << std::endl;
        lgGpiochipClose(h_gpio);
        return 1;
    }

    lgGpioWrite(h_gpio, PIN_LED, 0);
    lgGpioWrite(h_gpio, PIN_BUZZER, 0);

    std::cout << "Teste do LED..." << std::endl;
    for (int i = 0; i < 5; i++) {
        lgGpioWrite(h_gpio, PIN_LED, 1);
        usleep(500000);
        lgGpioWrite(h_gpio, PIN_LED, 0);
        usleep(500000);
    }

    std::cout << "Teste do buzzer..." << std::endl;
    for (int i = 0; i < 5; i++) {
        lgGpioWrite(h_gpio, PIN_BUZZER, 1);
        usleep(500000);
        lgGpioWrite(h_gpio, PIN_BUZZER, 0);
        usleep(500000);
    }

    lgGpioWrite(h_gpio, PIN_LED, 0);
    lgGpioWrite(h_gpio, PIN_BUZZER, 0);

    lgGpiochipClose(h_gpio);
    return 0;
}
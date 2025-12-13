#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include "nrf24_avr.h"

#define HIGH 1
#define LOW  0

#define DEADZONE 60

// Mapeamento físico equivalente ao Arduino
#define LED1 3
#define LED2 5

// PORTC
#define JY 0
#define JX 1
#define JS 2
#define TRIGGER 3

/**
 * @brief Função equivalente ao map() do Arduino.
 *
 * @param x valor de entrada
 * @param in_min faixa mínima de entrada
 * @param in_max faixa máxima de entrada
 * @param out_min faixa mínima de saída
 * @param out_max faixa máxima de saída
 * @return valor mapeado proporcionalmente
 */
long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief Estrutura enviada/recebida pelo controle RF.
 *
 * x e y variam de -127 a 127.
 * sw e trigger são botões (0 ou 1).
 */
typedef struct {
    int8_t x;
    int8_t y;
    int8_t sw;
    int8_t trigger;
} Controls;
static_assert(sizeof(Controls) == 4);

const uint8_t address[5] = {'0','0','0','0','1'};

/**
 * @brief Inicializa o ADC do AVR para leitura dos analógicos.
 */
void adc_setup() {
    ADMUX = (1 << REFS0); 
    ADCSRA = (1 << ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0); 
}

/**
 * @brief Lê o ADC em um canal específico.
 *
 * @param ch canal analógico (0–7)
 * @return valor de 0 a 1023
 */
uint16_t adc_read(uint8_t ch) {
    ADMUX = (ADMUX & 0xF0) | (ch & 0x07);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

/**
 * @brief Configura PWM dos pinos LED1 e LED2.
 */
void pwm_setup() {
    // LED1 -> PD3 (OC2B)
    DDRD |= (1 << 3);
    TCCR2A = (1<<WGM21) | (1<<WGM20) | (1<<COM2B1);
    TCCR2B = (1<<CS21);   // prescaler 8

    // LED2 -> PD5 (OC0B)
    DDRD |= (1 << PD5);
    TCCR0A = (1<<WGM01) | (1<<WGM00) | (1<<COM0B1);
    TCCR0B = (1<<CS01);   // prescaler 8
}

/**
 * @brief Escreve valor PWM em LED1 ou LED2.
 *
 * @param pin 3 ou 5
 * @param value intensidade 0–255
 */
void pwm_write(uint8_t pin, uint8_t value) {
    if (pin == 3)      OCR2B = value;
    else if (pin == 5) OCR0B = value;
}

/**
 * @brief Retorna valor absoluto de um inteiro.
 */
int abs_int(int n) { return n >= 0 ? n : -n; }

void timer0_setup() {
    sei();
    TCCR0A = 0;
    TCCR0B = (1 << CS02) | (1 << CS00); // Prescaler de 1024
    TIMSK0 = (1 << TOIE0);
    TCNT0 = 0;
}

volatile uint8_t ovf_count = 0;
ISR(TIMER0_OVF_vect) {
    ovf_count++;
}

void delay20ms() {
    ovf_count = 0;
    TCNT0 = 0;

    // Espera 1 ovf + 56 counts = 20ms
    while (ovf_count < 1);
    while (TCNT0 < 56);
}
    
/**
 * @brief Inicializações principais (ADC, PWM, entradas e rádio).
 */
void setup() {
    timer0_setup();
    
    adc_setup();
    pwm_setup();

    // JS e TRIGGER como entradas pull-up
    DDRC &= ~((1<<JS) | (1<<TRIGGER));
    PORTC |= ((1<<JS) | (1<<TRIGGER));

    // Rádio
    nrf24_begin(9, 10, RF24_SPI_SPEED);
    nrf24_openWritingPipe(address);
    nrf24_stopListening();
}

/**
 * @brief Loop principal: lê controles, aplica deadzone, envia por RF e atualiza LEDs.
 */
void loop() {
    Controls gamepad;

    gamepad.x = (int8_t) map(adc_read(JX), 0, 1023, -127, 127);
    gamepad.y = (int8_t) map(adc_read(JY), 0, 1023, -127, 127);

    gamepad.sw = (int8_t)(!(PINC & (1<<JS)));
    gamepad.trigger = (int8_t)(!(PINC & (1<<TRIGGER)));

    // DEADZONE
    if (gamepad.x > -DEADZONE && gamepad.x < DEADZONE) gamepad.x = 0;
    if (gamepad.y > -DEADZONE && gamepad.y < DEADZONE) gamepad.y = 0;

    uint8_t ok = nrf24_write(&gamepad, sizeof(gamepad));

    pwm_write(LED2, ok);
    pwm_write(LED1, abs_int(gamepad.y) * 2);

    delay20ms();
}

/**
 * @brief Função principal do firmware.
 */
int main() {
    setup();
    while (1) loop();
}

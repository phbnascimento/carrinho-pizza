#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "nrf24_avr.h"

#define HIGH 1
#define LOW  0

// Mapeamento físico equivalente ao Arduino
#define LED1 3
#define LED2 5

// PORTC
#define JY 0
#define JX 1
#define JS 2
#define TRIGGER 3

// map() igual ao Arduino
long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

typedef struct {
    int8_t x;
    int8_t y;
    int8_t sw;
    int8_t trigger;
} Controls;
static_assert(sizeof(Controls) == 4);

const uint8_t address[5] = {'0','0','0','0','1'};

void adc_setup() {
    ADMUX = (1 << REFS0); 
    ADCSRA = (1 << ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0); 
}

uint16_t adc_read(uint8_t ch) {
    ADMUX = (ADMUX & 0xF0) | (ch & 0x07);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

void pwm_setup() {
    // LED1 -> PD3 (OC2B)
    DDRD |= (1 << PD3);
    TCCR2A = (1<<WGM21) | (1<<WGM20) | (1<<COM2B1);
    TCCR2B = (1<<CS21);   // prescaler 8

    // LED2 -> PD5 (OC0B)
    DDRD |= (1 << PD5);
    TCCR0A = (1<<WGM01) | (1<<WGM00) | (1<<COM0B1);
    TCCR0B = (1<<CS01);   // prescaler 8
}

void pwm_write(uint8_t pin, uint8_t value) {
    if (pin == 3)      OCR2B = value;
    else if (pin == 5) OCR0B = value;
}

int abs_int(int n) { return n >= 0 ? n : -n; }

void setup() {

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

void loop() {
    Controls gamepad;

    gamepad.x = (int8_t) map(adc_read(JX), 0, 1023, -127, 127);
    gamepad.y = (int8_t) map(adc_read(JY), 0, 1023, -127, 127);

    gamepad.sw = (int8_t)(!(PINC & (1<<JS)));
    gamepad.trigger = (int8_t)(!(PINC & (1<<TRIGGER)));

    // DEADZONE 
    if (gamepad.x > -75 && gamepad.x < 75) gamepad.x = 0;
    if (gamepad.y > -75 && gamepad.y < 75) gamepad.y = 0;

    uint8_t ok = nrf24_write(&gamepad, sizeof(gamepad));

    pwm_write(LED2, ok);
    pwm_write(LED1, abs_int(gamepad.y) * 2);

    _delay_ms(20);
}

int main() {
    setup();
    while (1) { loop(); }
}

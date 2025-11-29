#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "nrf24_avr.h"

const uint8_t addr[5] = {'0', '0', '0', '0', '1'};

// PORTD
#define LED(LEDNUM, STATE) (STATE ? (PORTC |=  (1 << (LEDNUM))) : (PORTC &= ~(1 << (LEDNUM))))
#define LED1 3
#define BUZZ 4
#define LED2 5

// PORTB
#define JX 0  // Joystick X
#define JY 1 // Y
#define JS 2 // Switch
#define TRIGGER 3 // Botão na parte superior do controle

typedef struct {
  int8_t x;
  int8_t y;
  int8_t sw;
  int8_t trigger;
} Controls;
static_assert(sizeof(Controls) == 4);

int map(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void analog_setup(void) {
  // Setando para leitura nas portas analógicas
  ADMUX = (1 << REFS0);
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

  // Setup PWM PD5
  DDRD |= (1 << PD5);
  TCCR0A = (1<<WGM00) | (1<<WGM01) | (1<<COM0B1);
  TCCR0B = (1<<CS01);  // prescaler 8

  // Setup PWM PD3
  DDRD |= (1 << PD3);
  TCCR2A = (1<<WGM20) | (1<<WGM21) | (1<<COM2B1);
  TCCR2B = (1<<CS21); // prescaler 8
}

uint16_t analog_read(uint8_t channel) {
    channel &= 0x07;
    ADMUX = (ADMUX & 0xF0) | channel;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));

    return ADC;
}

void analog_write(uint8_t pin, uint8_t value) {
    if (pin == 6)      OCR0A = value;  // D6
    else if (pin == 3) OCR2B = value;  // D3
}

void loop() {
  Controls gamepad;

  gamepad.x = (int8_t) map(analog_read(JX), 0, 1023, -128, 127);
  gamepad.y = (int8_t) map(analog_read(JY), 0, 1023, -128, 127);
  gamepad.sw = (int8_t) !(PINB & (1<<JS));
  gamepad.trigger = (int8_t) !(PINB & (1<<TRIGGER));

  int ok = nrf24_write(&gamepad, sizeof(gamepad));
  LED(LED1, ok);
  analog_write(LED2, gamepad.x);

  _delay_ms(20);
}

int main() {
  // Rádio
  nrf24_begin(9, 10, RF24_SPI_SPEED);
  nrf24_openWritingPipe(addr);
	nrf24_setChannel(76);
  nrf24_stopListening();

  analog_setup();

  // LEDs
  DDRD |= (1<<LED1) | (1<<LED2) | (1<<BUZZ);
  PORTD &= ~(1<<BUZZ);

  // Define botão do joystick e trigger como pullup
  PORTB |= (1<<JS) | (1<<TRIGGER);

  while (1) { loop(); }

  return 0;
}
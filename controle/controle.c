#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "nrf24_avr.h"

const uint8_t addr[5] = {'0', '0', '0', '0', '1'};

// PORTD
#define LED1 3
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

void setup() {
  nrf24_begin(9, 10, RF24_SPI_SPEED);
  nrf24_openWritingPipe(addr);
	nrf24_setChannel(76);
  nrf24_stopListening();

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  pinMode(JS,  INPUT_PULLUP);
  pinMode(TRIGGER, INPUT_PULLUP);
}

void loop() {
  Controls gamepad;

  gamepad.x = (int8_t) map(analogRead(JX), 0, 1023, -127, 128);
  gamepad.y = (int8_t) map(analogRead(JY), 0, 1023, -127, 128);
  gamepad.sw = (int8_t) !digitalRead(JS);
  gamepad.trigger = (int8_t) !(PINB & (1<<TRIGGER));

  bool ok = nrf24_write(&gamepad, sizeof(gamepad));
  digitalWrite(LED1, ok);

  delay(20);
}
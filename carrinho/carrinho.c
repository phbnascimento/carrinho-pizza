#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "nrf24_avr.h"

#define HIGH 1
#define LOW  0

#define LDR 0

#define LED(LEDNUM, STATE) (STATE ? (PORTC |=  (1 << (LEDNUM))) : (PORTC &= ~(1 << (LEDNUM))))
#define LED1 1
#define LED2 2
#define LED3 3

#define IN2 1
#define IN1 2
#define ENB 3
#define IN4 4
#define IN3 5
#define ENA 6

#define IS_PRESSED (!(PIND & (1<<7)))
#define BTN 7

#define LASER 0

const uint8_t addr[5] = {'0', '0', '0', '0', '1'};

typedef struct {
  int8_t x;
  int8_t y;
  int8_t sw;
  int8_t trigger;
} Controls;
static_assert(sizeof(Controls) == 4);

typedef struct {
  long duration;
  long count;
} Timer;

uint8_t life = 0b1110;
bool on=false, pressed=false, prev=false;
bool ldr_prev = false;

Timer ldrTimer = {5000, 0};
Timer laserTimer = {1000, 0};

unsigned long millis_count = 0;

void timer_tick() {
  _delay_ms(1);
  millis_count++;
}

unsigned long millis() {
  return millis_count;
}

int abs(int num) {
  return num >= 0 ? num : num * -1;
}

bool isTimerOver(Timer t) {
  return millis() - t.count > t.duration;
}

void timerReset(Timer *t) {
  t->count = millis();
}

void analog_setup(void) {
  ADMUX = (1 << REFS0);
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

  DDRD |= (1 << PD6);
  TCCR0A = (1<<WGM00) | (1<<WGM01) | (1<<COM0A1);
  TCCR0B = (1<<CS01);

  DDRD |= (1 << PD3);
  TCCR2A = (1<<WGM20) | (1<<WGM21) | (1<<COM2B1);
  TCCR2B = (1<<CS21);
}

uint16_t analog_read(uint8_t channel) {
    channel &= 0x07;
    ADMUX = (ADMUX & 0xF0) | channel;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

void analog_write(uint8_t pin, uint8_t value) {
    if (pin == 6)      OCR0A = value;
    else if (pin == 3) OCR2B = value;
}

typedef enum {LEFT, RIGHT} Motor;
typedef enum {BACKWARDS=0, FORWARD=1} Dir;

void motor(Motor motor, Dir dir, uint8_t value);
void motor(Motor motor, Dir dir, uint8_t value) {
    switch (motor) {
    case LEFT:
        if (dir)  PORTD |=  (1 << IN1);
        else      PORTD &= ~(1 << IN1);

        if (!dir) PORTD |=  (1 << IN2);
        else      PORTD &= ~(1 << IN2);

        analog_write(ENA, value);
        break;
    case RIGHT:
        if (dir)  PORTD |=  (1 << IN3);
        else      PORTD &= ~(1 << IN3);

        if (!dir) PORTD |=  (1 << IN4);
        else      PORTD &= ~(1 << IN4);

        analog_write(ENB, value);
        break;
    }
}

void hit() {
  life <<= 1;
  if (life == 0b01110000) {
    motor(LEFT,  FORWARD,   200);
    motor(RIGHT, BACKWARDS, 200);
    _delay_ms(1000);
    motor(LEFT,  FORWARD,   0);
    motor(RIGHT, BACKWARDS, 0);
    _delay_ms(4000);
    life = 0b1110;
  }
}

void loop() {
  timer_tick();

  pressed = IS_PRESSED;
  if (pressed && !prev) on = !on;
  prev = pressed;

  uint16_t ldr_val = analog_read(LDR);
  bool ldr = ldr_val > 800;

  if (ldr && !ldr_prev) hit();
  ldr_prev = ldr;

  // Acender o laser
  if (isTimerOver(laserTimer)) {
    timerReset(&laserTimer);
    PORTB ^= (1<<0);
  }

  Controls gamepad;
  int available = nrf24_available();
  LED(LED2, available);

  if (available) {
    nrf24_read(&gamepad, sizeof(gamepad));
  }

  // Vida

  PORTC = life;

  // Controle do Motor
  if (gamepad.x < -100) {
    motor(LEFT, FORWARD, 0);
  } else {
    motor(LEFT, gamepad.y > 0 ? FORWARD : BACKWARDS, abs(gamepad.y) * 2);
  }

  if (gamepad.x > 100) {
    motor(RIGHT, FORWARD, 0);
  } else {
    motor(RIGHT, gamepad.y > 0 ? FORWARD : BACKWARDS, abs(gamepad.y) * 2);
  }
}

int main() {
  analog_setup();

  DDRB |= 0b00000001;
  DDRC |= 0b00001110;
  DDRD |= 0b01111110;

  PORTD |= (1<<7);

  nrf24_begin(9, 10, RF24_SPI_SPEED);
  nrf24_openReadingPipe(0, addr);
  nrf24_setChannel(76);
  nrf24_startListening();

  motor(LEFT, FORWARD, 0);
  motor(RIGHT, FORWARD, 0);

  while (1) loop();

  return 0;
}

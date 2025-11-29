#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "nrf24_avr.h"

#define HIGH 1
#define LOW  0

#define LDR 0

#define LED(LEDNUM, STATE) (STATE ? (PORTC |=  (1 << (LEDNUM))) : (PORTC &= ~(1 << (LEDNUM)))) // Macrozinho pra mudar o estado dos leds
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

#define LASER 0 // PORTB!!

/*
* -> OUTPUT
LDR A0
LED1 A1 *
LED2 A2 *
LED3 A3 *
IN2 1 *
IN1 2 *
ENB 3 *
IN4 4 *
IN3 5 *
ENA 6 *
BTN 7 *
LASER 8 *
*/

const uint8_t addr[5] = {'0', '0', '0', '0', '1'};

typedef struct {
  int8_t x;
  int8_t y;
  int8_t sw;
  int8_t trigger;
} Controls;
static_assert(sizeof(Controls) == 4);  // Só pra ter certeza (não to louco!)

uint8_t life = 0b1110;
bool on=false, pressed=false, prev=false;

// Função para inicializar leitura analógica e fast pwm
void analog_setup(void) {
  // Setando para leitura nas portas analógicas
  ADMUX = (1 << REFS0);
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

  // Setup PWM PD6
  DDRD |= (1 << PD6);
  TCCR0A = (1<<WGM00) | (1<<WGM01) | (1<<COM0A1); // Fast PWM, não-invertido
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

// Acabou para o Beta
void gameOver() {
  motor(LEFT,  FORWARD,   200);
  motor(RIGHT, BACKWARDS, 200);
  // Travar por 5s
  life = 0b1110;
} 

int abs(int num) {
  return num >= 0 ? num : num * -1;
}

void loop() {
  // Botão "just pressed"
  pressed = IS_PRESSED; // Pra sincronizar todos os valores, pois a leitura pode falhar.
  if (pressed && !prev) {
    on = !on;
  };
  prev = pressed;

  Controls gamepad;
  int available = nrf24_available();
  LED(LED2, available);
  LED(LED1, HIGH);

  if (available) {
   nrf24_read(&gamepad, sizeof(gamepad));
  }

  // LED(LED1, gamepad.sw);
  motor(LEFT,  gamepad.x > 0 ? FORWARD : BACKWARDS, 2 * abs(gamepad.x) - 1);
  motor(RIGHT, gamepad.x > 0 ? FORWARD : BACKWARDS, 2 * abs(gamepad.x) - 1);
}

int main() {
  analog_setup();

  // Preferi fazer assim pela facilidade
  DDRB |= 0b00000001;
  DDRC |= 0b00001110;
  DDRD |= 0b01111110;

  PORTD |= (1<<7); // Botão em pullup

  // Rádio
  nrf24_begin(9, 10, RF24_SPI_SPEED);
	nrf24_openReadingPipe(0, addr);
	nrf24_setChannel(76);
	// nrf24_setPayloadSize(4);
	nrf24_startListening();

  // Garantir que os motores iniciem parados
  motor(LEFT, FORWARD, 0);
  motor(RIGHT, FORWARD, 0);

  while (1) { loop(); }

  return 0;
}
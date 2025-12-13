#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
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

/**
 * @brief Estrutura recebida do controle remoto contendo eixos e botões.
 * @note Tamanho garantido em 4 bytes.
 */
typedef struct {
  int8_t x;
  int8_t y;
  int8_t sw;
  int8_t trigger;
} Controls;
static_assert(sizeof(Controls) == 4);

uint8_t life = 0b1110;
bool on=false, pressed=false, prev=false;
bool ldr_prev = false;

/**
 * @brief Retorna o valor absoluto de um inteiro.
 */
int abs(int num) {
  return num >= 0 ? num : num * -1;
}

void timer1_setup() {
    TCCR1A = 0;
    TCCR1B = (1<<CS12) | (1<<CS10);   // Prescaler de 1024
    TCNT1 = 49911;
    TIMSK1 = (1<<TOIE1);
}

volatile uint8_t ovf_count = 0;
ISR(TIMER1_OVF_vect) {
  TCNT1 = 49911; // preload para a espera de 1s
  ovf_count++;
}

void delay_sec(uint8_t seconds) {
    ovf_count = 0;
    TCNT1 = 49911;

    while (ovf_count < seconds);
}

/**
 * @brief Configura o ADC e os PWM usados.
 */
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

/**
 * @brief Lê um canal analógico (ADC0–ADC7).
 * @param channel Canal a ler.
 * @return Valor de 0 a 1023.
 */
uint16_t analog_read(uint8_t channel) {
    channel &= 0x07;
    ADMUX = (ADMUX & 0xF0) | channel;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

/**
 * @brief Define o duty de um PWM em uma das saídas configuradas.
 * @param pin Pino PWM (3 ou 6).
 * @param value Duty-cycle (0–255).
 */
void analog_write(uint8_t pin, uint8_t value) {
    if (pin == 6)      OCR0A = value;
    else if (pin == 3) OCR2B = value;
}

/**
 * @brief Enum dos motores disponíveis.
 */
typedef enum {LEFT, RIGHT} Motor;

/**
 * @brief Sentido de rotação: para frente ou para trás.
 */
typedef enum {BACKWARDS=0, FORWARD=1} Dir;

/**
 * @brief Controla um motor no sentido desejado e com PWM.
 * @param motor Motor LEFT ou RIGHT.
 * @param dir Direção FORWARD ou BACKWARDS.
 * @param value Intensidade PWM (0–255).
 */
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

/**
 * @brief Reduz a vida e executa a penalidade de dano.
 */
void hit() {
  life <<= 1;

  // Game Over
  if (life == 0b01110000) {
    PORTB &= ~(1<<0); // Desliga laser
    // Gira por 1s
    motor(LEFT,  FORWARD,   200);
    motor(RIGHT, BACKWARDS, 200);
    delay_sec(1);
    motor(LEFT,  FORWARD,   0);
    motor(RIGHT, BACKWARDS, 0);
    delay_sec(4); // espera os 4 segundos restantes para contar 5s
    life = 0b1110;
  }
}

/**
 * @brief Laço principal contendo toda a lógica do robô.
 */
void loop() {
  pressed = IS_PRESSED;
  if (pressed && !prev) on = !on;
  prev = pressed;

  bool ldr = (analog_read(LDR) > 800);

  if (ldr && !ldr_prev) hit();
  ldr_prev = ldr;

  // Recebendo os dados do controle
  Controls gamepad = {0, 0, 1, 1}; // desativa tudo caso a recepção falhe
  int available = nrf24_available();
  LED(LED2, available);
  
  if (available) {
    nrf24_read(&gamepad, sizeof(gamepad));
  }

  // Alterna o laser a cada 1s
  if (ovf_count >= 1) {
    ovf_count = 0;

    PORTB ^= (1<<0);
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

/**
 * @brief Função principal: inicializa periféricos e roda loop().
 */
int main() {
  timer1_setup();
  analog_setup();

  sei();

  // Preferi definir desse jeito pela facilidade
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

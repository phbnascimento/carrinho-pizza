#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define LDR A0
#define LED1 A1
#define LED2 A2
#define LED3 A3

#define IN2 1
#define IN1 2
#define ENB 3
#define IN4 4
#define IN3 5
#define ENA 6
#define BTN 7
#define LASER 8

#define IS_PRESSED (!digitalRead(BTN))

RF24 radio(9, 10);
const byte addr[6] = "00001";

typedef struct {
  long int duration;
  long int count;
} Timer;

typedef struct {
  int8_t x;
  int8_t y;
  int8_t sw;
  int8_t trigger;
} Controls;
static_assert(sizeof(Controls) == 4);

bool isTimerOver(Timer timer) {
  return millis() - timer.count > timer.duration;
}

void timerReset(Timer *timer) {
  timer->count = millis();
}

Timer ldrTimer, laserTimer;
uint8_t life = 0b1110;
bool on=false, pressed=false, prev=false;

typedef enum {LEFT, RIGHT} Motor;
typedef enum {BACKWARDS=0, FORWARD=1} Dir;
void motor(Motor motor, Dir dir, uint8_t value);
void motor(Motor motor, Dir dir, uint8_t value) {
  switch (motor) {
  case LEFT:
    digitalWrite(IN1, dir);
    digitalWrite(IN2, !dir);
    analogWrite(ENA, value);
    break;
  case RIGHT:
    digitalWrite(IN3, dir);
    digitalWrite(IN4, !dir);
    analogWrite(ENB, value);
    break;
  }
}

// Acabou para o Beta
void gameOver() {
  // Gira 180 e desliga por 5s
  motor(LEFT,  FORWARD,   200);
  motor(RIGHT, BACKWARDS, 200);
  delay(1000);
  motor(LEFT,  FORWARD,   0);
  motor(RIGHT, BACKWARDS, 0);
  delay(4000);

  // Reseta a vida
  life = 0b1110;
}

void motor_setup() {
  DDRD |= (1<<PD3) | (1<<PD6);
  OCR0A = 0;
  OCR2B = 0;
  TCCR0A = (1<<COM0A1) | (1<<COM0A0) | (1<<WGM01) | (1<<WGM00);
  TCCR2A = (1<<COM2B1) | (1<<COM2B0) | (1<<WGM21) | (1<<WGM20);
  TCCR0B = (1<<CS01) | (1<<CS00);
  TCCR2B = (1<<CS22);
}

void setup() {
  // Rádio
  radio.begin();
  radio.openReadingPipe(0, addr);
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening();

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(LASER, OUTPUT);

  pinMode(BTN, INPUT_PULLUP);

  ldrTimer   = {5000, 0};
  laserTimer = {1000, 0};

  motor(LEFT, FORWARD, 0);
  motor(RIGHT, FORWARD, 0);
}

void hit() {
  life <<= 1;

  if (life == 0b01110000) gameOver();
}

bool ldr_prev = false;
void loop() {
  // Botão "just pressed"
  pressed = IS_PRESSED; // Para sincronizar todos os valores, pois a leitura pode falhar.
  if (pressed && !prev) {
    on = !on;
  };
  prev = pressed;

  // Leitura LDR
  bool ldr = analogRead(LDR) > 800;
  if (ldr && ! ldr_prev) {
    hit();
  }
  ldr_prev = ldr;
  
  if (isTimerOver(ldrTimer)) {
    timerReset(&ldrTimer);
    PORTB ^= (1<<0); // acende e apaga a cada 1s
  }
  
  Controls gamepad;
  bool available = radio.available();
  digitalWrite(LED2, available);

  if (available) {
    radio.read(&gamepad, sizeof(gamepad));
  }

  // Vida
  PORTC = life;

  // Controle do Motor
  if (gamepad.x < -100) {
    motor(LEFT,  FORWARD, 0);
  } else {
    motor(LEFT,  gamepad.y > 0 ? FORWARD : BACKWARDS, abs(gamepad.y) * 2);
  }

  if (gamepad.x > 100) {
    motor(RIGHT,  FORWARD, 0);

  } else {
    motor(RIGHT,  gamepad.y > 0 ? FORWARD : BACKWARDS, abs(gamepad.y) * 2);
  }
}
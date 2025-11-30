  #include <SPI.h>
  #include <nRF24L01.h>
  #include <RF24.h>

  RF24 radio(9, 10); 
  const byte address[6] = "00001";

  #define LED1 3
  #define LED2 5

  #define JY A0
  #define JX A1
  #define JS A2
  #define TRIGGER A3

  typedef struct {
    int8_t x;
    int8_t y;
    int8_t sw;
    int8_t trigger;
  } Controls;
  static_assert(sizeof(Controls) == 4);

  void setup() {
    radio.begin();
    radio.openWritingPipe(address);
    radio.setPALevel(RF24_PA_LOW);
    radio.stopListening();

    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);

    pinMode(JS,  INPUT_PULLUP);
    pinMode(TRIGGER, INPUT_PULLUP);
  }

  void loop() {
    Controls gamepad;

    gamepad.x = (int8_t) map(analogRead(JX), 0, 1023, -127, 127);
    gamepad.y = (int8_t) map(analogRead(JY), 0, 1023, -127, 127);
    gamepad.sw = (int8_t) (!digitalRead(JS));
    gamepad.trigger = (int8_t) (!digitalRead(TRIGGER));

    // DEADZONE 
    if (gamepad.x > -75 && gamepad.x < 75) gamepad.x = 0;
    if (gamepad.y > -75 && gamepad.y < 75) gamepad.y = 0;

    bool ok = radio.write(&gamepad, sizeof(gamepad));
    analogWrite(LED2, ok);
    analogWrite(LED1, (abs(gamepad.y) * 2));

    delay(20);
  }
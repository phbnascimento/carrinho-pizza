/**
 * @file main_controller.c
 * @brief Firmware do Receptor do Carrinho de Controle Remoto (AVR/ATmega328P).
 *
 * @details Este arquivo implementa a lógica de recepção de sinal via NRF24L01,
 * controle de ponte H (Motores DC) via PWM e leitura de sensores analógicos.
 * O código utiliza manipulação direta de registradores para máxima performance.
 *
 * @author Seu Nome (Preencha Aqui)
 * @date 2025-11-29
 * @version 1.1.0 (Beta - Comentado para Doxygen)
 *
 * @see nrf24_avr.h Para detalhes da biblioteca de rádio.
 * @see https://www.microchip.com/en-us/product/ATmega328P Datasheet do MCU.
 */

#ifndef F_CPU
/** @brief Define a frequência da CPU para 16MHz se não definida pelo compilador. */
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "nrf24_avr.h"

/* --- Definições de Lógica --- */

/** @def HIGH
 * @brief Nível lógico alto (1). */
#define HIGH 1

/** @def LOW
 * @brief Nível lógico baixo (0). */
#define LOW  0

/* --- Mapeamento de Hardware (Pinos) --- */

/** @def LDR
 * @brief Canal ADC para o sensor de luz (Analog 0). */
#define LDR 0

/**
 * @def LED(LEDNUM, STATE)
 * @brief Macro funcional para controlar LEDs no PORTC.
 *
 * Utiliza operações bitwise para setar (|=) ou limpar (&= ~) o bit correspondente.
 * @param LEDNUM O índice do pino no PORTC (ex: 1, 2, 3).
 * @param STATE Estado desejado: HIGH (1) ou LOW (0).
 */
#define LED(LEDNUM, STATE) (STATE ? (PORTC |=  (1 << (LEDNUM))) : (PORTC &= ~(1 << (LEDNUM))))

/** @def LED1
 * @brief Pino do LED indicativo 1 (PC1 / A1). */
#define LED1 1

/** @def LED2
 * @brief Pino do LED indicativo 2 (PC2 / A2). */
#define LED2 2

/** @def LED3
 * @brief Pino do LED indicativo 3 (PC3 / A3). */
#define LED3 3

/* --- Pinos da Ponte H (Motor Driver) --- */
/** @def IN2
 * @brief Controle de direção Motor A (PD1). */
#define IN2 1
/** @def IN1
 * @brief Controle de direção Motor A (PD2). */
#define IN1 2
/** @def ENB
 * @brief Enable/PWM do Motor B (PD3 - OC2B). */
#define ENB 3
/** @def IN4
 * @brief Controle de direção Motor B (PD4). */
#define IN4 4
/** @def IN3
 * @brief Controle de direção Motor B (PD5). */
#define IN3 5
/** @def ENA
 * @brief Enable/PWM do Motor A (PD6 - OC0A). */
#define ENA 6

/* --- Interface de Entrada --- */
/**
 * @def IS_PRESSED
 * @brief Verifica se o botão foi pressionado (Lógica Invertida/Pull-up).
 *
 * Como o botão está em pull-up, PIND & (1<<7) retorna 0 quando pressionado.
 * O operador '!' inverte para retornar TRUE.
 * @return 1 se pressionado, 0 se solto.
 */
#define IS_PRESSED (!(PIND & (1<<7)))

/** @def BTN
 * @brief Pino do Botão de usuário (PD7). */
#define BTN 7

/** @def LASER
 * @brief Pino de controle do Laser (PB0). */
#define LASER 0 // PORTB!!

/**
 * @var addr
 * @brief Endereço do pipe de leitura do rádio NRF24.
 * Endereço: "00001".
 */
const uint8_t addr[5] = {'0', '0', '0', '0', '1'};

/* --- Estruturas de Dados --- */

/**
 * @struct Controls
 * @brief Estrutura do payload recebido via rádio.
 * Mapeia os dados enviados pelo controle remoto (transmissor).
 */
typedef struct {
  int8_t x;       /**< Eixo X do Joystick (-127 a 127). */
  int8_t y;       /**< Eixo Y do Joystick (-127 a 127). */
  int8_t sw;      /**< Botão do Joystick (Switch). */
  int8_t trigger; /**< Gatilho ou botão auxiliar. */
} Controls;

/**
 * @enum Motor
 * @brief Seleção do motor esquerdo ou direito.
 */
typedef enum {
    LEFT,  /**< Motor Esquerdo (Controlado por IN1/IN2/ENA). */
    RIGHT  /**< Motor Direito (Controlado por IN3/IN4/ENB). */
} Motor;

/**
 * @enum Dir
 * @brief Direção de rotação do motor.
 */
typedef enum {
    BACKWARDS=0, /**< Ré. */
    FORWARD=1    /**< Frente. */
} Dir;

/* --- Variáveis Globais de Estado --- */

/** @brief Vida atual do jogador (representação binária para LEDs). Inicial: 0b1110. */
uint8_t life = 0b1110;

/** @brief Estado de funcionamento do sistema (Ligado/Desligado). */
bool on=false;

/** @brief Estado atual de leitura do botão (para debounce). */
bool pressed=false;

/** @brief Estado anterior de leitura do botão (para detecção de borda). */
bool prev=false;

/* --- Implementação das Funções --- */

/**
 * @brief Configura o ADC e os Timers para PWM.
 *
 * Configurações detalhadas:
 * - **ADC**: Referência AVCC, Prescaler 128 (para clock de 16MHz).
 * - **Timer0 (PD6)**: Fast PWM, Non-Inverting, Prescaler 8.
 * - **Timer2 (PD3)**: Fast PWM, Non-Inverting, Prescaler 8.
 */
void analog_setup(void) {
  // Setando para leitura nas portas analógicas (ADC)
  // REFS0: Referência de tensão no pino AVCC com capacitor em AREF
  ADMUX = (1 << REFS0);
  // ADEN: Enable ADC, ADPS[2:0]: Prescaler division factor (ajuste de clock)
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

  // Setup PWM PD6 (Timer0 - 8 bits)
  DDRD |= (1 << PD6); // Configura PD6 como saída
  // WGM00/01: Fast PWM mode | COM0A1: Clear OC0A on Compare Match (non-inverting)
  TCCR0A = (1<<WGM00) | (1<<WGM01) | (1<<COM0A1);
  TCCR0B = (1<<CS01);  // CS01: Prescaler 8 (frequência do PWM adequada p/ motores)

  // Setup PWM PD3 (Timer2 - 8 bits)
  DDRD |= (1 << PD3); // Configura PD3 como saída
  // WGM20/21: Fast PWM mode | COM2B1: PWM no pino OC2B (PD3)
  TCCR2A = (1<<WGM20) | (1<<WGM21) | (1<<COM2B1);
  TCCR2B = (1<<CS21); // CS21: Prescaler 8
}

/**
 * @brief Realiza a leitura analógica de um canal específico.
 *
 * @param channel Canal ADC a ser lido (0 a 7).
 * @return uint16_t Valor convertido (0 a 1023).
 * @note Esta função é bloqueante (aguarda o fim da conversão).
 */
uint16_t analog_read(uint8_t channel) {
    channel &= 0x07; // Garante segurança (apenas canais 0-7)
    // Limpa os bits do canal antigo (0xF0) e define o novo (| channel)
    ADMUX = (ADMUX & 0xF0) | channel;
    // Inicia a conversão (Start Conversion)
    ADCSRA |= (1 << ADSC);
    // Aguarda o bit ADSC ir para 0 (fim da conversão)
    while (ADCSRA & (1 << ADSC));

    return ADC; // Retorna o registrador de 16 bits
}

/**
 * @brief Escreve um valor PWM em um pino específico.
 *
 * Abstrai a escrita nos registradores de comparação (OCR0A e OCR2B).
 * @param pin Pino de destino (6 ou 3).
 * @param value Valor do Duty Cycle (0 a 255).
 */
void analog_write(uint8_t pin, uint8_t value) {
    if (pin == 6)      OCR0A = value;  // Timer0 A (PD6)
    else if (pin == 3) OCR2B = value;  // Timer2 B (PD3)
}

/**
 * @brief Controla a direção e velocidade de um motor DC.
 *
 * Gerencia a lógica da Ponte H setando os pinos INx e o pino de Enable.
 *
 * @param motor Enum Motor (LEFT ou RIGHT).
 * @param dir Enum Dir (FORWARD ou BACKWARDS).
 * @param value Velocidade PWM (0-255).
 */
void motor(Motor motor, Dir dir, uint8_t value) {
    switch (motor) {
    case LEFT:
        // Lógica para Motor Esquerdo (IN1/IN2)
        if (dir)  PORTD |=  (1 << IN1);
        else      PORTD &= ~(1 << IN1);

        if (!dir) PORTD |=  (1 << IN2);
        else      PORTD &= ~(1 << IN2);

        analog_write(ENA, value);
        break;
    case RIGHT:
        // Lógica para Motor Direito (IN3/IN4)
        if (dir)  PORTD |=  (1 << IN3);
        else      PORTD &= ~(1 << IN3);

        if (!dir) PORTD |=  (1 << IN4);
        else      PORTD &= ~(1 << IN4);

        analog_write(ENB, value);
        break;
    }
}

/**
 * @brief Executa a rotina de "Game Over".
 *
 * Faz o carrinho girar no próprio eixo (um motor para frente, outro para trás)
 * e reseta a variável de vida.
 */
void gameOver() {
  motor(LEFT,  FORWARD,   200);
  motor(RIGHT, BACKWARDS, 200);
  // @todo Implementar delay não bloqueante ou usar Timer
  // Travar por 5s (Lógica futura)
  life = 0b1110;
}

/**
 * @brief Função auxiliar para calcular valor absoluto.
 * @param num Inteiro com sinal.
 * @return int Valor absoluto (positivo).
 */
int abs(int num) {
  return num >= 0 ? num : num * -1;
}

/**
 * @brief Loop principal de lógica do sistema.
 *
 * Realiza as seguintes tarefas:
 * 1. Leitura e debounce do botão de start.
 * 2. Verificação de dados disponíveis no rádio NRF24.
 * 3. Processamento do pacote de dados (Struct Controls).
 * 4. Atuação nos motores.
 *
 * @warning Atualmente contém código de teste (hardcoded) sobrescrevendo a entrada de rádio.
 */
void loop() {
  // --- Lógica de Botão (Edge Detection) ---
  pressed = IS_PRESSED; // Sincroniza leitura
  if (pressed && !prev) {
    on = !on; // Toggle estado
  };
  prev = pressed;

  // --- Lógica de Rádio ---
  Controls gamepad;
  int available = nrf24_available();

  // LED de Debug: Acende se houver dados de rádio
  // LED(LED2, available);
  PORTC |= (1 << LED2); // Força LED2 ligado para teste

  if (available) {
    nrf24_read(&gamepad, sizeof(gamepad));
  }

  // @note SOBRESCRITA PARA TESTE: Força x=100 ignorando o rádio
  gamepad.x = 100;

  // Código comentado (Lógica real futura):
  // LED(LED1, gamepad.sw);
  // motor(LEFT,  gamepad.x > 0 ? FORWARD : BACKWARDS, abs(gamepad.x));
  // motor(RIGHT, gamepad.x > 0 ? FORWARD : BACKWARDS, abs(gamepad.x));

  // --- Teste Direto de Motores ---
  PORTD |= (1<<IN1);
  PORTD &= ~(1<<IN2);
  analog_write(ENA, 200);
}

/**
 * @brief Ponto de entrada do programa (Setup e Loop Infinito).
 *
 * Configurações de Hardware:
 * - DDRB: PB0 como Saída.
 * - DDRC: PC1, PC2, PC3 como Saída (LEDs).
 * - DDRD: Pinos da Ponte H como Saída, Botão como Entrada.
 * - Pull-up ativado no pino do botão.
 * - Inicialização do Rádio NRF24L01.
 *
 * @return int Retorno padrão (nunca alcançado em sistemas embarcados).
 */
int main() {
  // Configuração de Direção dos Pinos (Data Direction Register)
  DDRB |= 0b00000001; // PB0 Output (Laser)
  DDRC |= 0b00001110; // PC1-PC3 Output (LEDs)
  DDRD |= 0b01111110; // PD1-PD6 Output (Motores), PD7 Input

  // Ativa resistor de Pull-up interno no pino PD7 (Botão)
  PORTD |= (1 << 7);

  // Inicializa periféricos
  analog_setup();

  // Configuração do Rádio NRF24
  // CE no pino 9 (PB1), CSN no pino 10 (PB2)
  nrf24_begin(9, 10, RF24_SPI_SPEED);
  nrf24_openReadingPipe(0, addr);
  nrf24_setChannel(76); // Canal RF 76
  // nrf24_setPayloadSize(4); // Opcional: Definir tamanho fixo
  nrf24_startListening();

  // Loop Infinito
  while (1) { loop(); }

  return 0;
}
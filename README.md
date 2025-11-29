# Gigantes de MDF - CARRO-PIZZA! 🚗

## Introdução
Bem-vindo à documentação técnica do projeto de **Carrinho-Pizza**. 
Este firmware foi desenvolvido para a arquitetura AVR (ATmega328P) utilizando manipulação direta de registradores ("Bare Metal") para garantir a máxima eficiência no tempo de resposta dos motores.

### 🎯 Objetivos
* Fazer controle PWM e temporizadores via Timers de Hardware (Timer0 e Timer2).
* Implementar protocolo de comunicação sem fio com o módulo de rádio NRF24L01.
* Demonstrar conhecimentos no desenvolvimento com microcontroladores.

---

## 🛠️ Hardware Utilizado

| Componente | Especificação | Função |
| :--- | :--- | :--- |
| **MCU** | ATmega328P (16MHz) | "Cérebro" do sistema |
| **Rádio** | NRF24L01+ | Comunicação 2.4GHz |
| **Driver** | Ponte H (L298N) | Controle de potência dos motores |
| **Sensores** |  LDR | Detecção de luz do ambiente |

---

## 🔌 Pinagem (Pinout)

Abaixo está o mapeamento físico dos pinos do microcontrolador para os periféricos:

* **Motores:**
    * `PD6 (OC0A)`: PWM Motor Esquerdo
    * `PD3 (OC2B)`: PWM Motor Direito
    * `PD1/PD2/PD4/PD5`: Controle de Direção (Ponte H)
* **Comunicação:**
    * `PB1/PB2`: Controle do Rádio (CE/CSN)
    * `SPI`: Padrão do ATmega
* **Interface:**
    * `PD7`: Botão para debug (Pull-up)
    * `PC1-PC3`: LEDs de "Vida" do carrinho

---

## 🚀 Como Compilar

1.  Abra o arquivo `Makefile`.
1.  Configure o `PORT` para a porta USB correta onde será feita a transmissão do código.
2.  Compile apenas utilizando o comando `make DIR=<carrinho/controle>`.

> **Nota:** Certifique-se de que a biblioteca `nrf24_avr.h` esteja presente nos dois diretórios.

---

**Autores:** Bruno Garcia Carvalho, Pedro Henrique Brito, Pedro Henrique Cretella  
**Disciplina:** Programação de Hardware / Microcontroladores  
**Data:** Novembro 2025
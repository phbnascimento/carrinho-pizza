#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include "nrf24_avr.h"

#define CE_PIN 8
#define CSN_PIN 10

uint8_t rxaddr[5] = {'N','O','D','E','1'};

int main(void) {
	// LED no D13 (PB5)
	DDRC |= (1 << PC4);

	nrf24_begin(CE_PIN, CSN_PIN, RF24_SPI_SPEED);
	nrf24_setChannel(76);
	nrf24_setPayloadSize(1);
	nrf24_openReadingPipe(0, rxaddr);
	nrf24_startListening();

	uint8_t buf[1];

	while(1) {
		if (nrf24_available()) {
			nrf24_read(buf, 1);
			if (buf[0] == 0x55) {
				PORTC |= (1 << PC4);  // acende LED
				_delay_ms(1000);
				PORTC &= ~(1 << PC4); // apaga LED
			}
		}
	}
}

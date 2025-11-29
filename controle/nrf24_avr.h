#ifndef NRF24_AVR_H
#define NRF24_AVR_H

#include <stdint.h>
#include "nRF24L01.h"
#include "RF24_config.h"

// API (funções diretas)
void nrf24_init_hwspi(void); // configura SPI hardware
void nrf24_begin(uint8_t ce_pin, uint8_t csn_pin, uint32_t spi_speed_hz);
void nrf24_setChannel(uint8_t channel);
void nrf24_setPayloadSize(uint8_t size);
void nrf24_openWritingPipe(const uint8_t *address); // address length = 5
void nrf24_openReadingPipe(uint8_t pipe, const uint8_t *address); // only pipe 0 used here
void nrf24_startListening(void);
void nrf24_stopListening(void);
uint8_t nrf24_write(const void *buf, uint8_t len);
uint8_t nrf24_available(void);
void nrf24_read(void *buf, uint8_t len);
void nrf24_flush_tx(void);
void nrf24_flush_rx(void);
uint8_t nrf24_getStatus(void);

#endif

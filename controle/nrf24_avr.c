#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include "nrf24_avr.h"
#include "nRF24L01.h"
#include "RF24_config.h"

/* ---------- Arduino-style digital pin to AVR port helpers (UNO mapping) ----------
 D0-D7  -> PORTD (PD0..PD7)
 D8-D13 -> PORTB (PB0..PB5)
 A0-A5  -> PORTC (PC0..PC5)  (if needed)
---------------------------------------------------------------------------------*/

static inline void pinMode_d(uint8_t dpin, uint8_t mode) {
    if (dpin <= 7) {
        if (mode) DDRD |= (1 << dpin); else DDRD &= ~(1 << dpin);
    } else if (dpin <= 13) {
        uint8_t b = dpin - 8;
        if (mode) DDRB |= (1 << b); else DDRB &= ~(1 << b);
    } else { /* not handled */ }
}

static inline void digitalWrite_d(uint8_t dpin, uint8_t val) {
    if (dpin <= 7) {
        if (val) PORTD |= (1 << dpin); else PORTD &= ~(1 << dpin);
    } else if (dpin <= 13) {
        uint8_t b = dpin - 8;
        if (val) PORTB |= (1 << b); else PORTB &= ~(1 << b);
    }
}

static inline uint8_t digitalRead_d(uint8_t dpin) {
    if (dpin <= 7) {
        return (PIND >> dpin) & 1;
    } else if (dpin <= 13) {
        uint8_t b = dpin - 8;
        return (PINB >> b) & 1;
    }
    return 0;
}

/* SPI hardware helpers */
static void spi_init(void) {
    // MOSI (PB3) output, SCK (PB5) output, SS (PB2) output as CSN default
    DDRB |= (1<<PB3)|(1<<PB5)|(1<<PB2);
    // Enable SPI, Master, set clock rate fck/2 (SPI2X=1, SPR0=0 SPR1=0)
    SPCR = (1<<SPE)|(1<<MSTR);
    SPSR = (1<<SPI2X);
}

static uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while(!(SPSR & (1<<SPIF)));
    return SPDR;
}

/* nRF control pins */
static uint8_t _ce_pin = 8;
static uint8_t _csn_pin = 10;
static uint8_t payload_size = 32;
static uint8_t addr_width = 5;
static uint8_t dynamic_payloads = 1;

/* CSN / CE wrappers */
static inline void csn_low(void)  { digitalWrite_d(_csn_pin, 0); _delay_us(1); }
static inline void csn_high(void) { digitalWrite_d(_csn_pin, 1); _delay_us(1); }
static inline void ce_low(void)   { digitalWrite_d(_ce_pin, 0); }
static inline void ce_high(void)  { digitalWrite_d(_ce_pin, 1); }

/* Low-level register access */
static uint8_t status_reg;
static void write_reg(uint8_t reg, const uint8_t* buf, uint8_t len) {
    csn_low();
    status_reg = spi_transfer(W_REGISTER | (reg & REGISTER_MASK));
    for (uint8_t i=0;i<len;i++) spi_transfer(buf[i]);
    csn_high();
}
static uint8_t read_reg(uint8_t reg) {
    uint8_t rv;
    csn_low();
    status_reg = spi_transfer(R_REGISTER | (reg & REGISTER_MASK));
    rv = spi_transfer(0xff);
    csn_high();
    return rv;
}
static void read_reg_buf(uint8_t reg, uint8_t* buf, uint8_t len) {
    csn_low();
    status_reg = spi_transfer(R_REGISTER | (reg & REGISTER_MASK));
    for (uint8_t i=0;i<len;i++) buf[i] = spi_transfer(0xff);
    csn_high();
}

/* payload ops */
static void write_payload(const void* buf, uint8_t len, uint8_t writeType) {
    const uint8_t* p = (const uint8_t*)buf;
    csn_low();
    status_reg = spi_transfer(writeType);
    uint8_t copy = len;
    if (!dynamic_payloads) {
        if (copy > payload_size) copy = payload_size;
        for (uint8_t i=0;i<copy;i++) spi_transfer(*p++);
        for (uint8_t i=copy;i<payload_size;i++) spi_transfer(0);
    } else {
        if (copy > 32) copy = 32;
        for (uint8_t i=0;i<copy;i++) spi_transfer(*p++);
    }
    csn_high();
}

static void read_payload(void* buf, uint8_t len) {
    uint8_t* p = (uint8_t*)buf;
    csn_low();
    status_reg = spi_transfer(R_RX_PAYLOAD);
    uint8_t copy = len;
    if (!dynamic_payloads) {
        if (copy > payload_size) copy = payload_size;
        for (uint8_t i=0;i<copy;i++) *p++ = spi_transfer(0xff);
        for (uint8_t i=copy;i<payload_size;i++) spi_transfer(0xff);
    } else {
        if (copy > 32) copy = 32;
        for (uint8_t i=0;i<copy;i++) *p++ = spi_transfer(0xff);
    }
    csn_high();
}

/* API implementation */

void nrf24_init_hwspi(void) {
    spi_init();
}

void nrf24_begin(uint8_t ce_pin, uint8_t csn_pin, uint32_t spi_speed_hz) {
    _ce_pin = ce_pin;
    _csn_pin = csn_pin;
    pinMode_d(_ce_pin, 1);
    pinMode_d(_csn_pin, 1);
    ce_low(); csn_high();
    nrf24_init_hwspi();

    // Basic reset/config
    _delay_us(RF24_POWERUP_DELAY);
    // power up, CRC 1 byte, PRIM_RX=0
    uint8_t cfg = (1<<PWR_UP) | (1<<EN_CRC);
    write_reg(NRF_CONFIG, &cfg, 1);
    _delay_ms(5);
    // default payload size
    nrf24_setPayloadSize(payload_size);
    // enable auto ack on pipe0
    uint8_t en_aa = 0x01;
    write_reg(EN_AA, &en_aa, 1);
    // enable pipe0
    uint8_t en_rx = 0x01;
    write_reg(EN_RXADDR, &en_rx, 1);
}

void nrf24_setChannel(uint8_t channel) {
    if (channel > 125) channel = 125;
    write_reg(RF_CH, &channel, 1);
}

void nrf24_setPayloadSize(uint8_t size) {
    if (size < 1) size = 1;
    if (size > 32) size = 32;
    payload_size = size;
    uint8_t p = payload_size;
    for (uint8_t i=0; i<6; i++) write_reg(RX_PW_P0 + i, &p, 1);
}

void nrf24_openWritingPipe(const uint8_t *address) {
    // TX_ADDR and pipe0 read address must be same for ACKs
    write_reg(TX_ADDR, address, addr_width);
    write_reg(RX_ADDR_P0, address, addr_width);
}

void nrf24_openReadingPipe(uint8_t pipe, const uint8_t *address) {
    if (pipe == 0) {
        write_reg(RX_ADDR_P0, address, addr_width);
    } else if (pipe >=1 && pipe <=5) {
        // for pipes 1..5 only LSB stored, here we write full for simplicity
        write_reg(RX_ADDR_P0 + pipe, address, 1);
    }
}

void nrf24_startListening(void) {
    // set PRIM_RX bit
    uint8_t cfg = read_reg(NRF_CONFIG);
    cfg |= (1<<PRIM_RX);
    write_reg(NRF_CONFIG, &cfg, 1);
    ce_high();
    _delay_us(130); // allow radio to enter RX
}

void nrf24_stopListening(void) {
    ce_low();
    // clear PRIM_RX
    uint8_t cfg = read_reg(NRF_CONFIG);
    cfg &= ~(1<<PRIM_RX);
    write_reg(NRF_CONFIG, &cfg, 1);
    _delay_us(130);
}

uint8_t nrf24_write(const void *buf, uint8_t len) {
    nrf24_stopListening();
    // write payload
    write_payload(buf, len, W_TX_PAYLOAD);
    // pulse CE to transmit (130us)
    ce_high();
    _delay_us(15);
    ce_low();
    // Wait for TX_DS or MAX_RT
    // naive busy wait with small timeout
    uint16_t timeout = 5000; // ~5ms * loops => ~? conservative
    while (timeout--) {
        uint8_t status = read_reg(NRF_STATUS);
        if (status & (1<<TX_DS)) {
            // clear flag
            uint8_t clear = (1<<TX_DS);
            write_reg(NRF_STATUS, &clear, 1);
            return 1; // success
        } else if (status & (1<<MAX_RT)) {
            uint8_t clear = (1<<MAX_RT);
            write_reg(NRF_STATUS, &clear, 1);
            nrf24_flush_tx();
            return 0; // failed
        }
        _delay_us(10);
    }
    // timeout
    return 0;
}

uint8_t nrf24_available(void) {
    uint8_t status = read_reg(NRF_STATUS);
    if (status & (1<<RX_DR)) return 1;
    return 0;
}

void nrf24_read(void *buf, uint8_t len) {
    read_payload(buf, len);
    // clear RX_DR
    uint8_t clear = (1<<RX_DR);
    write_reg(NRF_STATUS, &clear, 1);
}

void nrf24_flush_tx(void) {
    csn_low();
    spi_transfer(FLUSH_TX);
    csn_high();
}

void nrf24_flush_rx(void) {
    csn_low();
    spi_transfer(FLUSH_RX);
    csn_high();
}

uint8_t nrf24_getStatus(void) {
    csn_low();
    uint8_t s = spi_transfer(RF24_NOP);
    csn_high();
    return s;
}

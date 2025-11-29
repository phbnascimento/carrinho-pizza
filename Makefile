MCU = atmega328p
F_CPU = 16000000UL
PORT = /dev/ttyACM0
PROGRAMMER = arduino
BAUD = 115200

ifndef DIR
$(error Use: make DIR=pasta)
endif

SRC = $(wildcard $(DIR)/*.c) $(wildcard $(DIR)/*.cpp)
OBJ = $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)
TARGET = $(DIR)/firmware

CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall
CXXFLAGS = $(CFLAGS)
LDFLAGS = -mmcu=$(MCU)

all: hex upload

$(TARGET).elf: $(OBJ)
	avr-gcc $(LDFLAGS) $^ -o $@

%.o: %.c
	avr-gcc $(CFLAGS) -c $< -o $@

%.o: %.cpp
	avr-g++ $(CXXFLAGS) -c $< -o $@

hex: $(TARGET).elf
	avr-objcopy -O ihex -R .eeprom $(TARGET).elf $(TARGET).hex

upload:
	avrdude -C /etc/avrdude.conf -p $(MCU) -c $(PROGRAMMER) -P $(PORT) -b $(BAUD) -U flash:w:$(TARGET).hex:i

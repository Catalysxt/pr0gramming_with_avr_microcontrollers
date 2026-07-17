
##########------------------------------------------------------##########
##########              Project-specific Details                ##########
##########------------------------------------------------------##########

MCU   = atmega328p
F_CPU = 16000000UL

## Shared driver library — sibling of this project folder
LIBDIR = ../extern_libraries

##########------------------------------------------------------##########
##########                 Programmer Defaults                  ##########
##########------------------------------------------------------##########

PROGRAMMER_TYPE = usbasp
PROGRAMMER_ARGS =

##########------------------------------------------------------##########
##########                  Program Locations                   ##########
##########------------------------------------------------------##########

CC      = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
AVRSIZE = avr-size
AVRDUDE = avrdude

##########------------------------------------------------------##########
##########                   Makefile Magic                     ##########
##########------------------------------------------------------##########

TARGET = $(lastword $(subst /, ,$(CURDIR)))

SOURCES  = $(wildcard src/*.c)
SOURCES += $(LIBDIR)/lcd_driver/lcd_driver.c
OBJECTS  = $(SOURCES:.c=.o)
HEADERS  = $(wildcard src/*.h)

CPPFLAGS = -DF_CPU=$(F_CPU) -I. -I$(LIBDIR)/lcd_driver -Isrc
CFLAGS   = -Os -g -std=gnu99 -Wall -Wextra -Wpedantic -Werror
CFLAGS  += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS  += -ffunction-sections -fdata-sections
LDFLAGS  = -Wl,-Map,$(TARGET).map
LDFLAGS += -Wl,--gc-sections
TARGET_ARCH = -mmcu=$(MCU)

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<;

$(TARGET).elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LDLIBS) -o $@

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.eeprom: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@

%.lst: %.elf
	$(OBJDUMP) -S $< > $@

.PHONY: all disassemble disasm eeprom size clean squeaky_clean flash fuses

all: $(TARGET).hex

debug:
	@echo
	@echo "Source files:" $(SOURCES)
	@echo "MCU, F_CPU:" $(MCU), $(F_CPU)
	@echo

disassemble: $(TARGET).lst
disasm: disassemble

size: $(TARGET).elf
	$(AVRSIZE) $(TARGET).elf

clean:
	rm -f $(TARGET).elf $(TARGET).hex $(TARGET).obj \
	$(TARGET).o $(TARGET).d $(TARGET).eep $(TARGET).lst \
	$(TARGET).lss $(TARGET).sym $(TARGET).map $(TARGET)~ \
	$(TARGET).eeprom

squeaky_clean:
	rm -f *.elf *.hex *.obj *.o *.d *.eep *.lst *.lss *.sym *.map *~ *.eeprom
	rm -f src/*.o $(LIBDIR)/lcd_driver/*.o

##########------------------------------------------------------##########
##########              Programmer-specific details             ##########
##########------------------------------------------------------##########

flash: $(TARGET).hex
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -U flash:w:$<

program: flash

flash_eeprom: $(TARGET).eeprom
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -U eeprom:w:$<

avrdude_terminal:
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -nt

##########------------------------------------------------------##########
##########       Fuse settings — external 16 MHz crystal        ##########
##########------------------------------------------------------##########

## LFUSE=0xF7: external full-swing crystal, slow startup (safe)
## HFUSE=0xD9: SPIEN enabled, EESAVE disabled (EEPROM erased on chip-erase)
## EFUSE=0x07: BOD disabled
## To preserve EEPROM across flash cycles, use set_eeprom_save_fuse first.
LFUSE = 0xF7
HFUSE = 0xD9
EFUSE = 0x07

FUSE_STRING = -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m -U efuse:w:$(EFUSE):m

fuses:
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) $(FUSE_STRING)

show_fuses:
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -nv

set_default_fuses: FUSE_STRING = -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m -U efuse:w:$(EFUSE):m
set_default_fuses: fuses

## Set HFUSE=0xD7 to preserve EEPROM across chip-erase / re-flash
set_eeprom_save_fuse: HFUSE = 0xD7
set_eeprom_save_fuse: FUSE_STRING = -U hfuse:w:$(HFUSE):m
set_eeprom_save_fuse: fuses

clear_eeprom_save_fuse: FUSE_STRING = -U hfuse:w:$(HFUSE):m
clear_eeprom_save_fuse: fuses

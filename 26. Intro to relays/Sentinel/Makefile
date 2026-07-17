
##########------------------------------------------------------##########
##########              Project-specific Details                ##########
##########------------------------------------------------------##########

MCU   = atmega328p
F_CPU = 16000000UL

## Shared drivers live in the workspace-root extern_libraries/ (a sibling of
## every project). relay/sentinel/ -> relay/ -> my_avr_programming/extern_libraries/
LIBDIR = ../../extern_libraries

## Build output directory — all generated files land here, never in project root
BUILDDIR = build

## Make 'all' the default goal so a bare 'make' produces the .hex
.DEFAULT_GOAL := all

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
SOURCES += $(LIBDIR)/USART.c
SOURCES += $(LIBDIR)/pin/pin.c
OBJECTS  = $(SOURCES:.c=.o)
HEADERS  = $(wildcard src/*.h)

CPPFLAGS = -DF_CPU=$(F_CPU) -I. -I$(LIBDIR) -I$(LIBDIR)/pin -Isrc
## -Wpedantic dropped: shared extern_libraries/USART.c uses GCC binary
## literals (0b...) which -Wpedantic rejects under -std=gnu99. Driver left
## untouched per extern_libraries discipline; -Wall -Wextra -Werror kept.
CFLAGS   = -Os -g -std=gnu99 -Wall -Wextra -Werror
CFLAGS  += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS  += -ffunction-sections -fdata-sections
LDFLAGS  = -Wl,-Map,$(BUILDDIR)/$(TARGET).map
LDFLAGS += -Wl,--gc-sections
TARGET_ARCH = -mmcu=$(MCU)

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<;

$(BUILDDIR)/$(TARGET).elf: $(OBJECTS) | $(BUILDDIR)
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LDLIBS) -o $@

$(BUILDDIR)/$(TARGET).hex: $(BUILDDIR)/$(TARGET).elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

$(BUILDDIR)/$(TARGET).eeprom: $(BUILDDIR)/$(TARGET).elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@

$(BUILDDIR)/$(TARGET).lst: $(BUILDDIR)/$(TARGET).elf
	$(OBJDUMP) -S $< > $@

$(BUILDDIR):
	mkdir -p $@

.PHONY: all disassemble disasm eeprom size clean squeaky_clean flash fuses

all: $(BUILDDIR)/$(TARGET).hex

debug:
	@echo
	@echo "Source files:" $(SOURCES)
	@echo "MCU, F_CPU:" $(MCU), $(F_CPU)
	@echo

disassemble: $(BUILDDIR)/$(TARGET).lst
disasm: disassemble

size: $(BUILDDIR)/$(TARGET).elf
	$(AVRSIZE) $<

clean:
	rm -rf $(BUILDDIR)

squeaky_clean:
	rm -rf $(BUILDDIR)
	rm -f src/*.o $(LIBDIR)/*.o $(LIBDIR)/pin/*.o

##########------------------------------------------------------##########
##########              Programmer-specific details             ##########
##########------------------------------------------------------##########

flash: $(BUILDDIR)/$(TARGET).hex
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -U flash:w:$<

program: flash

flash_eeprom: $(BUILDDIR)/$(TARGET).eeprom
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

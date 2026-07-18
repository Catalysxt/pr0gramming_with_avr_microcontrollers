
##########------------------------------------------------------##########
##########              Project-specific Details                ##########
##########------------------------------------------------------##########

MCU   = atmega328p
F_CPU = 16000000UL

## Shared drivers live in a sibling extern_libraries/ folder.
## (slot_machine/ -> max7219,max7221/ -> my_avr_programming/ -> extern_libraries/)
LIBDIR = ../../extern_libraries

## Build output directory — all generated files land here, never in project root
BUILDDIR = build

## Make 'all' the default goal so a bare 'make' produces the .hex
.DEFAULT_GOAL := all

##########------------------------------------------------------##########
##########                 Programmer Defaults                  ##########
##########------------------------------------------------------##########

## usbasp by default; override on the command line, e.g.
##   make flash PROGRAMMER_TYPE=arduino PROGRAMMER_ARGS="-P COM3 -b 115200"
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

TARGET = slot_machine

## Project sources: main + app FSM + project-local drivers.
SOURCES  = $(wildcard src/*.c) $(wildcard src/app/*.c) $(wildcard src/drivers/*.c)
## Shared drivers pulled from extern_libraries (never duplicated into src/).
SOURCES += $(LIBDIR)/pin/pin.c $(LIBDIR)/spi/spi.c
SOURCES += $(LIBDIR)/max7219/max7219.c $(LIBDIR)/font7seg/font7seg.c
OBJECTS  = $(SOURCES:.c=.o)
HEADERS  = $(wildcard src/*.h src/app/*.h src/drivers/*.h src/assets/*.h)

CPPFLAGS  = -DF_CPU=$(F_CPU)
CPPFLAGS += -Isrc -Isrc/app -Isrc/drivers -Isrc/assets
CPPFLAGS += -I$(LIBDIR)/pin -I$(LIBDIR)/spi -I$(LIBDIR)/max7219 -I$(LIBDIR)/font7seg

## -std=gnu99 (not c99) so GCC binary literals (0b...) in the assets compile.
CFLAGS   = -Os -g -std=gnu99 -Wall -Wextra -Werror
CFLAGS  += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS  += -ffunction-sections -fdata-sections
LDFLAGS  = -Wl,-Map,$(BUILDDIR)/$(TARGET).map
LDFLAGS += -Wl,--gc-sections
TARGET_ARCH = -mmcu=$(MCU)

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

## Print the size report automatically on every successful link.
$(BUILDDIR)/$(TARGET).elf: $(OBJECTS) | $(BUILDDIR)
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LDLIBS) -o $@
	@echo
	@$(AVRSIZE) $@

$(BUILDDIR)/$(TARGET).hex: $(BUILDDIR)/$(TARGET).elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

$(BUILDDIR)/$(TARGET).eeprom: $(BUILDDIR)/$(TARGET).elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@

$(BUILDDIR)/$(TARGET).lst: $(BUILDDIR)/$(TARGET).elf
	$(OBJDUMP) -S $< > $@

$(BUILDDIR):
	mkdir -p $@

.PHONY: all disassemble disasm size clean squeaky_clean flash program avrdude_terminal fuses show_fuses

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
	rm -f src/*.o src/app/*.o src/drivers/*.o
	rm -f $(LIBDIR)/pin/*.o $(LIBDIR)/spi/*.o $(LIBDIR)/max7219/*.o $(LIBDIR)/font7seg/*.o

##########------------------------------------------------------##########
##########              Programmer-specific details             ##########
##########------------------------------------------------------##########

flash: $(BUILDDIR)/$(TARGET).hex
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -U flash:w:$<

program: flash

avrdude_terminal:
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -nt

##########------------------------------------------------------##########
##########       Fuse settings — external 16 MHz crystal        ##########
##########------------------------------------------------------##########

## LFUSE=0xF7: external full-swing crystal, slow startup (safe)
## HFUSE=0xD9: SPIEN enabled, EESAVE disabled
## EFUSE=0x07: BOD disabled
LFUSE = 0xF7
HFUSE = 0xD9
EFUSE = 0x07

FUSE_STRING = -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m -U efuse:w:$(EFUSE):m

fuses:
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) $(FUSE_STRING)

show_fuses:
	$(AVRDUDE) -c $(PROGRAMMER_TYPE) -p $(MCU) $(PROGRAMMER_ARGS) -nv

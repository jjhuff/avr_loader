##############################################################################
#
#   avr_loader
#
#   AVR Hexfile Boot Loader
#
#   Copyright (C) 2012 Justin Huff

#    This is heavily based on avr_loader by Frank Edelhaeuser.
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
##############################################################################

#
#   Boot loader customization
#   =========================
#
#   MCU
#
#       AVR variant (atmega8, attiny85...). For a list of known MCU names, 
#       see the output of "avr-gcc --target-help".
#
#   F_CPU
#
#       CPU clock, in Hertz (depends on hardware clock source)
#
#   BOOTADDR
#
#       Specifies the start address of the boot loader in flash, in bytes. 
#       See AVR datasheet for a list of valid addresses. Please note that 
#       the datasheet usually specifies word addresses (e.g. word address 
#       0xF00 is byte address 0x1E00).
#
#       Make sure the compiled boot loader fits into the flash address
#       space above $(BOOTADDR). Inspect the boot loader hex file for the
#       largest used flash address.
#
#   FUSE_L
#   FUSE_H
#
#       Fuse low and high bytes. See AVR datasheet. Fuse settings shall 
#       match with the other settings. In particular:
#
#           - CLKSEL and SUT should match the hardware clock
#             circuit and should result in the CPU clock specified 
#             with $(F_CPU.)
#
#           - BOOTSZ should match the boot loader start address 
#             specified with $(BOOTADDR).
#
#           - BOOTRST should be set to 0.
#
#   BAUD
#
#       Specifies the baud rate to use for serial communication. Make 
#       sure your clock configuration supports this baud rate without\
#       significant error.
#
#   DUDEPORT
#
#       Specifies avrdude command line parameters for configuring your 
#       programmer. At least your programmer type (-c <programmer>), 
#       and any port or timing parameters it may require should be 
#       provided. The makefile automatically appends the CPU model 
#       from $(MCU).
#
MCU       = atmega328p
F_CPU     = 16000000
BAUD      = 9600

# 512-byte bootloader (see comments above)
BOOTADDR  = 0x7e00
FUSE_L    = 0xff
FUSE_H    = 0xde

# Your programmer, your serial port, your main app...
DUDEPORT  = -c buspirate -P /dev/bus_pirate

#
# Anything below should NOT require modification
# ==============================================
#
PROJECT  = avr_loader
LFLAGS   = -Wl,-Ttext,$(BOOTADDR),--relax -mmcu=$(MCU) -nodefaultlibs -nostartfiles
CFLAGS   = -g -Os -fno-inline-small-functions -fno-split-wide-types -mshort-calls -Wall -mmcu=$(MCU) -I. -DF_CPU=$(F_CPU) -DTIMEOUT=$(TIMEOUT) -DBAUD=$(BAUD) -DBOOTADDR=$(BOOTADDR)
AVRDUDE  = avrdude $(DUDEPORT) -p $(MCU) -y -u -V
OBJS     = avr_loader.o


all: avr_loader.hex

clean:
	rm -f *.hex *.lst *.map *.elf *.o

fuse:
	$(AVRDUDE) -U hfuse:w:$(FUSE_H):m -U lfuse:w:$(FUSE_L):m

flash: avr_loader.hex
	$(AVRDUDE) -U flash:w:$^:i

.c.o:
	avr-gcc -c $(CFLAGS) $< -o $@

%.elf: $(OBJS)
	avr-gcc $(LFLAGS) -Wl,-Map,$@.map -o $@ $^
	avr-objdump --source -d $@ > $@.lst
	avr-size --common -d $@

%.hex : %.elf
	avr-objcopy -j .text -O ihex $^ $@

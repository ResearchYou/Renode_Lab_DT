TARGET  = token
SRCDIR  = .
BUILDDIR = build

CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc
SIZE    = arm-none-eabi-size

CPU     = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

CFLAGS  = $(CPU) -O1 -g -Wall -Wextra \
          -ffunction-sections -fdata-sections \
          -ffreestanding -nostdlib \
          -I$(SRCDIR)

ASFLAGS = $(CPU) -g -x assembler-with-cpp

LDFLAGS = $(CPU) -T$(SRCDIR)/linker.ld \
          -Wl,--gc-sections \
          -Wl,-Map=$(BUILDDIR)/$(TARGET).map \
          -nostdlib -ffreestanding

OBJS    = $(BUILDDIR)/main.o $(BUILDDIR)/startup.o

.PHONY: all clean

all: $(BUILDDIR) $(BUILDDIR)/$(TARGET).elf

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/main.o: $(SRCDIR)/main.c $(SRCDIR)/stm32f4.h $(SRCDIR)/token_protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/startup.o: $(SRCDIR)/startup.s
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILDDIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@
	$(SIZE) $@

clean:
	rm -rf $(BUILDDIR)

TARGET  = spi_test
SRCDIR  = .
BUILDDIR = build

CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CPU     = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=soft

CFLAGS  = $(CPU) -O1 -g -Wall -Wextra \
          -ffunction-sections -fdata-sections \
          -ffreestanding -nostdlib \
          -I$(SRCDIR)

ASFLAGS = $(CPU) -g -x assembler-with-cpp

LDFLAGS = $(CPU) -T$(SRCDIR)/linker.ld \
          -Wl,--gc-sections \
          -Wl,-Map=$(BUILDDIR)/$(TARGET).map \
          -nostdlib -ffreestanding

SRCS    = $(SRCDIR)/main.c
ASRCS   = $(SRCDIR)/startup.s

OBJS    = $(BUILDDIR)/main.o $(BUILDDIR)/startup.o

.PHONY: all clean

all: $(BUILDDIR) $(BUILDDIR)/$(TARGET).elf

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/main.o: $(SRCDIR)/main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/startup.o: $(SRCDIR)/startup.s
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILDDIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@
	$(SIZE) $@

clean:
	rm -rf $(BUILDDIR)

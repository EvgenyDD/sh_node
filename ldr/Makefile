EXE_NAME=sh_nd_ldr
PRODUCT = sh_nd
VER_MAJ = 1
VER_MIN = 0
VER_PATCH = 0
MAKE_BINARY=yes

TCHAIN = arm-none-eabi-
MCPU += -mcpu=cortex-m3 -mthumb
CDIALECT = gnu99
OPT_LVL = 2
DBG_OPTS = -gdwarf-2 -ggdb -g

CFLAGS   += -fdata-sections -ffunction-sections 
CFLAGS   += -fsingle-precision-constant
CFLAGS   += -fmessage-length=0
CFLAGS   += -fno-exceptions -funsafe-math-optimizations
CFLAGS   += -fno-move-loop-invariants -ffreestanding
CFLAGS   += -Wno-pointer-sign -Wswitch-default -Wshadow -Wno-unused
CFLAGS   += -Wall -Wstrict-prototypes -Wdisabled-optimization -Wformat=2 -Winit-self  -Wmissing-include-dirs
CFLAGS   += -Wstrict-overflow=2
CFLAGS   += -Werror

EXT_LIBS +=c nosys

LDFLAGS  += -specs=nano.specs
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -Wl,--print-memory-usage

PPDEFS += STM32F103xB USE_STDPERIPH_DRIVER STM32F10X_MD FW_HEADER_REDUCED PROD_NAME=\"$(PROD_NAME)\" FW_TYPE=FW_LDR CFG_SYSTEM_SAVES_NON_NATIVE_DATA=1

INCDIR += .
INCDIR += ../common
INCDIR += ../common/CMSIS
INCDIR += ../common/crc
INCDIR += ../common/fw_header
INCDIR += ../common/canopennode
INCDIR += ../common/canopendriver
INCDIR += ../common/config_system
INCDIR += ../common/flasher
INCDIR += ../common/STM32F10x_StdPeriph_Driver/inc
SOURCES += $(call rwildcard, ../common, *.c *.S *.s)
SOURCES += ldr.c OD.c
LDSCRIPT += ../ldscript/ldr.ld

SIGNED_BINARY = $(EXE_NAME)_signed.bin

ARTEFACTS += $(SIGNED_BINARY)

include ../common/core.mk

signer:
	@make -C ../signer_fw_tool -j

$(SIGNED_BINARY): $(BINARY) signer
	@../signer_fw_tool/signer_fw_tool* $(BINARY) $(BUILDDIR)/$(SIGNED_BINARY) 24576 \
		prod=$(PRODUCT) \
		prod_name=$(EXE_NAME) \
		ver_maj=$(VER_MAJ) \
		ver_min=$(VER_MIN) \
		ver_pat=$(VER_PATCH) \
		build_ts=$$(date -u +'%Y%m%d_%H%M%S')

flash: $(SIGNED_BINARY)
	@openocd -f ../target/stm32_f103.cfg -c "program $(BUILDDIR)/$(SIGNED_BINARY) 0x08000C00 verify reset exit" 

debug:
	@echo "file $(EXECUTABLE)" > .gdbinit
	@echo "set auto-load safe-path /" >> .gdbinit
	@echo "set confirm off" >> .gdbinit
	@echo "target remote | openocd -c \"gdb_port pipe\" -f ../target/stm32_f103.cfg" >> .gdbinit
	@arm-none-eabi-gdb -q -x .gdbinit
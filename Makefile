EXE_NAME=sh_nd
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

LDFLAGS  += -specs=nano.specs
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -Wl,--print-memory-usage

EXT_LIBS +=c nosys

PPDEFS += STM32F103xB USE_STDPERIPH_DRIVER STM32F10X_MD PROD_NAME=\"$(PROD_NAME)\" FW_TYPE=FW_APP CFG_SYSTEM_SAVES_NON_NATIVE_DATA=1

INCDIR += .
INCDIR += app
INCDIR += app/adc
INCDIR += app/i2c
INCDIR += app/spi
INCDIR += app/uart
INCDIR += common
INCDIR += common/CMSIS
INCDIR += common/crc
INCDIR += common/fw_header
INCDIR += common/canopennode
INCDIR += common/canopendriver
INCDIR += common/config_system
INCDIR += common/flasher
INCDIR += common/STM32F10x_StdPeriph_Driver/inc
SOURCES += $(call rwildcard, common app, *.c *.S *.s)

LDSCRIPT += ldscript/app.ld

BINARY_SIGNED = $(BUILDDIR)/$(EXE_NAME)_app_signed.bin
BINARY_MERGED = $(BUILDDIR)/$(EXE_NAME)_full.bin

ARTEFACTS += $(BINARY_SIGNED) $(BINARY_MERGED)

include common/core.mk

signer:
	@make -C signer_fw_tool -j

$(BINARY_SIGNED): $(BINARY) signer
	@signer_fw_tool/signer_fw_tool* $(BUILDDIR)/$(EXE_NAME).bin $(BUILDDIR)/$(EXE_NAME)_app_signed.bin 102400 \
		prod=$(PRODUCT) \
		prod_name=$(EXE_NAME)_app \
		ver_maj=$(VER_MAJ) \
		ver_min=$(VER_MIN) \
		ver_pat=$(VER_PATCH) \
		build_ts=$$(date -u +'%Y%m%d_%H%M%S')

PRELDR_SIGNED = preldr/build/$(EXE_NAME)_preldr_signed.bin
LDR_SIGNED = ldr/build/$(EXE_NAME)_ldr_signed.bin 
EXT_MAKE_TARGETS = $(PRELDR_SIGNED) $(LDR_SIGNED)

clean: clean_ext_targets

.PHONY: $(EXT_MAKE_TARGETS)
$(EXT_MAKE_TARGETS):
	$(MAKE) -C $(subst build/,,$(dir $@))

.PHONY: clean_ext_targets
clean_ext_targets:
	$(foreach var,$(EXT_MAKE_TARGETS),$(MAKE) -C $(subst build/,,$(dir $(var))) clean;)

$(BINARY_MERGED): $(EXT_MAKE_TARGETS) $(BINARY_SIGNED)
	@echo " [Merging binaries] ..."
	@cp -f $(PRELDR_SIGNED) $@
	@dd if=$(LDR_SIGNED) of=$@ bs=1 oflag=append seek=3072 status=none
	@dd if=$(BINARY_SIGNED) of=$@ bs=1 oflag=append seek=28672 status=none

#####################
### FLASH & DEBUG ###
#####################

flash: $(BINARY_SIGNED)
	@openocd -f target/stm32_f103.cfg -c "program $(BINARY_SIGNED) 0x08007000 verify reset exit" 

flash_full: $(BINARY_MERGED)
	openocd -f target/stm32_f103.cfg -c "program $(BINARY_MERGED) 0x08000000 verify reset exit"

debug:
	@echo "file $(EXECUTABLE)" > .gdbinit
	@echo "set auto-load safe-path /" >> .gdbinit
	@echo "set confirm off" >> .gdbinit
	@echo "target remote | openocd -c \"gdb_port pipe\" -f target/stm32_f103.cfg" >> .gdbinit
	@arm-none-eabi-gdb -q -x .gdbinit
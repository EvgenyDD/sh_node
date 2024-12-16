EXE_NAME=sh_nd_ldr
PRODUCT =sh_nd
VER_MAJ = 1
VER_MIN = 0
VER_PATCH = 1
MAKE_BINARY=yes

TCHAIN = arm-none-eabi-
MCPU += -mcpu=cortex-m3 -mthumb
CDIALECT = gnu99
OPT_LVL = 2
DBG_OPTS = -gdwarf-2 -ggdb -g

CFLAGS   += -fvisibility=hidden -funsafe-math-optimizations -fdata-sections -ffunction-sections -fno-move-loop-invariants
CFLAGS   += -fmessage-length=0 -fno-exceptions -fno-common -fno-builtin -ffreestanding
CFLAGS   += -fsingle-precision-constant
CFLAGS   += $(C_FULL_FLAGS)
CFLAGS   += -Werror

CXXFLAGS += -fvisibility=hidden -funsafe-math-optimizations -fdata-sections -ffunction-sections -fno-move-loop-invariants
CXXFLAGS += -fmessage-length=0 -fno-exceptions -fno-common -fno-builtin -ffreestanding
CXXFLAGS += -fvisibility-inlines-hidden -fuse-cxa-atexit -felide-constructors -fno-rtti
CXXFLAGS += -fsingle-precision-constant
CXXFLAGS += $(CXX_FULL_FLAGS)
CXXFLAGS += -Werror

LDFLAGS  += -specs=nano.specs
LDFLAGS  += -Wl,--gc-sections
LDFLAGS  += -Wl,--print-memory-usage

EXT_LIBS +=c nosys

PPDEFS += STM32F103xB USE_STDPERIPH_DRIVER STM32F10X_MD FW_HEADER_REDUCED PROD_NAME=\"$(PROD_NAME)\" FW_TYPE=FW_LDR CFG_SYSTEM_SAVES_NON_NATIVE_DATA=1

PPDEFS += CO_USE_GLOBALS
PPDEFS += CO_CONFIG_FIFO=0
PPDEFS += CO_CONFIG_EM=0
PPDEFS += CO_CONFIG_GFC=0
PPDEFS += CO_CONFIG_GTW=0
PPDEFS += CO_CONFIG_HB_CONS=0
#PPDEFS += CO_CONFIG_LEDS=CO_CONFIG_LEDS_ENABLE
PPDEFS += CO_CONFIG_LEDS=0
PPDEFS += CO_CONFIG_LSS="CO_CONFIG_LSS_SLAVE|CO_CONFIG_LSS_SLAVE_FASTSCAN_DIRECT_RESPOND"
PPDEFS += CO_CONFIG_NMT=0
PPDEFS += CO_CONFIG_PDO=0
PPDEFS += CO_CONFIG_SDO_CLI=0
PPDEFS += CO_CONFIG_SDO_SRV="CO_CONFIG_SDO_SRV_SEGMENTED"
PPDEFS += CO_CONFIG_SDO_SRV_BUFFER_SIZE=534
PPDEFS += CO_CONFIG_SRDO=0
PPDEFS += CO_CONFIG_SYNC=0
PPDEFS += CO_CONFIG_TIME=0
PPDEFS += CO_CONFIG_TRACE=0

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

BINARY_SIGNED = $(BUILDDIR)/$(EXE_NAME)_signed.bin
BINARY_MERGED = $(BUILDDIR)/$(PRODUCT)_ldr_preldr_signed.bin
SIGN = $(BUILDDIR)/sign
ARTEFACTS += $(BINARY_SIGNED)

PRELDR_SIGNED = ../preldr/build/$(PRODUCT)_preldr_signed.bin
EXT_MAKE_TARGETS = $(PRELDR_SIGNED)

include ../common/core.mk

$(SIGN): ../sign/sign.c
	@gcc $< -o $@

$(BINARY_SIGNED): $(BINARY) $(SIGN)
	@$(SIGN) $(BINARY) $@ 24576 \
		prod=$(PRODUCT) \
		prod_name=$(EXE_NAME) \
		ver_maj=$(VER_MAJ) \
		ver_min=$(VER_MIN) \
		ver_pat=$(VER_PATCH) \
		build_ts=$$(date -u +'%Y%m%d_%H%M%S')

$(BINARY_MERGED): $(EXT_MAKE_TARGETS) $(BINARY_SIGNED)
	@echo " [Merging binaries] ..." {$@}
	@cp -f $< $@
	@dd if=$(BINARY_SIGNED) of=$@ bs=1 oflag=append seek=3072 status=none

.PHONY: composite
composite: $(BINARY_MERGED)

.PHONY: clean
clean: clean_ext_targets

.PHONY: clean_ext_targets
clean_ext_targets:
	$(foreach var,$(EXT_MAKE_TARGETS),$(MAKE) -C $(subst build/,,$(dir $(var))) clean;)

.PHONY: $(EXT_MAKE_TARGETS)
$(EXT_MAKE_TARGETS):
	@$(MAKE) -C $(subst build/,,$(dir $@)) --no-print-directory

#####################
### FLASH & DEBUG ###
#####################

flash: $(BINARY_SIGNED)
	@openocd -d0 -f ../target/stm32_f103.cfg -c "program $< 0x08000C00 verify reset exit" 

flash_all: $(BINARY_MERGED)
	@openocd -d0 -f ../target/stm32_f103.cfg -c "program $< 0x08000000 verify reset exit" 

ds:
	@openocd -d0 -f ../target/stm32_f103.cfg

debug:
	@set _NO_DEBUG_HEAP=1
	@echo "file $(EXECUTABLE)" > .gdbinit
	@echo "set auto-load safe-path /" >> .gdbinit
	@echo "set confirm off" >> .gdbinit
	@echo "target extended-remote :3333" >> .gdbinit
	@arm-none-eabi-gdb -q -x .gdbinit
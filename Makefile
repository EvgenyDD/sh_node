EXE_NAME=sh_nd
VER_MAJ = 1
VER_MIN = 0
VER_PATCH = 0
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

EXT_LIBS +=c m nosys

PPDEFS += STM32F103xB USE_STDPERIPH_DRIVER STM32F10X_MD PROD_NAME=\"$(PROD_NAME)\" FW_TYPE=FW_APP CFG_SYSTEM_SAVES_NON_NATIVE_DATA=1
PPDEFS += CO_USE_GLOBALS
PPDEFS += CO_CONFIG_FIFO=0
PPDEFS += CO_CONFIG_EM="CO_CONFIG_EM_PRODUCER|CO_CONFIG_EM_STATUS_BITS|CO_CONFIG_FLAG_TIMERNEXT"
PPDEFS += CO_CONFIG_GFC=0
PPDEFS += CO_CONFIG_GTW=0
PPDEFS += CO_CONFIG_HB_CONS="CO_CONFIG_HB_CONS_ENABLE|CO_CONFIG_FLAG_TIMERNEXT"
#PPDEFS += CO_CONFIG_LEDS=CO_CONFIG_LEDS_ENABLE
PPDEFS += CO_CONFIG_LEDS=0
PPDEFS += CO_CONFIG_LSS="CO_CONFIG_LSS_SLAVE|CO_CONFIG_LSS_SLAVE_FASTSCAN_DIRECT_RESPOND"
PPDEFS += CO_CONFIG_NMT=CO_CONFIG_FLAG_TIMERNEXT
PPDEFS += CO_CONFIG_PDO="CO_CONFIG_RPDO_ENABLE|CO_CONFIG_TPDO_ENABLE|CO_CONFIG_RPDO_TIMERS_ENABLE|CO_CONFIG_TPDO_TIMERS_ENABLE|CO_CONFIG_PDO_SYNC_ENABLE|CO_CONFIG_PDO_OD_IO_ACCESS|CO_CONFIG_FLAG_TIMERNEXT|CO_CONFIG_FLAG_OD_DYNAMIC"
PPDEFS += CO_CONFIG_SDO_CLI=0
PPDEFS += CO_CONFIG_SDO_SRV="CO_CONFIG_SDO_SRV_SEGMENTED|CO_CONFIG_FLAG_TIMERNEXT|CO_CONFIG_FLAG_OD_DYNAMIC"
PPDEFS += CO_CONFIG_SDO_SRV_BUFFER_SIZE=534
PPDEFS += CO_CONFIG_SRDO=0
PPDEFS += CO_CONFIG_SYNC="CO_CONFIG_SYNC_ENABLE|CO_CONFIG_FLAG_TIMERNEXT|CO_CONFIG_FLAG_OD_DYNAMIC"
PPDEFS += CO_CONFIG_TIME="CO_CONFIG_TIME_ENABLE|CO_CONFIG_FLAG_OD_DYNAMIC"
PPDEFS += CO_CONFIG_TRACE=0

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
SIGN = $(BUILDDIR)/sign
ARTEFACTS += $(BINARY_SIGNED)

PRELDR_SIGNED = preldr/build/$(EXE_NAME)_preldr_signed.bin
LDR_SIGNED = ldr/build/$(EXE_NAME)_ldr_signed.bin 
EXT_MAKE_TARGETS = $(PRELDR_SIGNED) $(LDR_SIGNED)

include common/core.mk

$(SIGN): sign/sign.c
	@gcc $< -o $@

$(BINARY_SIGNED): $(BINARY) $(SIGN)
	@$(SIGN) $(BINARY) $@ 102400 \
		prod=$(EXE_NAME) \
		prod_name=$(EXE_NAME)_app \
		ver_maj=$(VER_MAJ) \
		ver_min=$(VER_MIN) \
		ver_pat=$(VER_PATCH) \
		build_ts=$$(date -u +'%Y%m%d_%H%M%S')

$(BINARY_MERGED): $(EXT_MAKE_TARGETS) $(BINARY_SIGNED)
	@echo " [Merging binaries] ..." {$@}
	@cp -f $(PRELDR_SIGNED) $@
	@dd if=$(LDR_SIGNED) of=$@ bs=1 oflag=append seek=3072 status=none
	@dd if=$(BINARY_SIGNED) of=$@ bs=1 oflag=append seek=28672 status=none

.PHONY: composite
composite: $(BINARY_MERGED)

.PHONY: clean
clean: clean_ext_targets

.PHONY: $(EXT_MAKE_TARGETS)
$(EXT_MAKE_TARGETS):
	@$(MAKE) -C $(subst build/,,$(dir $@)) --no-print-directory

.PHONY: clean_ext_targets
clean_ext_targets:
	$(foreach var,$(EXT_MAKE_TARGETS),$(MAKE) -C $(subst build/,,$(dir $(var))) clean;)

#####################
### FLASH & DEBUG ###
#####################

flash: $(BINARY_SIGNED)
	openocd -d0 -f target/stm32_f103.cfg -c "program $< 0x08007000 verify reset exit" 

flash_full: $(BINARY_MERGED)
	openocd -d0 -f target/stm32_f103.cfg -c "program $< 0x08000000 verify reset exit"

debug:
	@echo "file $(EXECUTABLE)" > .gdbinit
	@echo "set auto-load safe-path /" >> .gdbinit
	@echo "set confirm off" >> .gdbinit
	@echo "target remote | openocd -c \"gdb_port pipe\" -f target/stm32_f103.cfg" >> .gdbinit
	@arm-none-eabi-gdb -q -x .gdbinit

define tftp_flash
	@atftp --verbose -p -r sh_nd_app$(2) -l $(3) 7.7.7.$(1)
endef

112: $(BINARY_SIGNED)
	$(call tftp_flash,11,2,$<)
113: $(BINARY_SIGNED)
	$(call tftp_flash,11,3,$<)

122: $(BINARY_SIGNED)
	$(call tftp_flash,12,2,$<)
124: $(BINARY_SIGNED)
	$(call tftp_flash,12,4,$<)

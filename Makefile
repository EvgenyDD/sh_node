SUBDIRS := $(wildcard apps/*)
EXTRA_ARGS =

.PHONY: all composite clean $(SUBDIRS)
all: $(SUBDIRS)
composite: EXTRA_ARGS = composite
composite: $(SUBDIRS)

clean:
	@rm -rf build
	@rm -rf ldr/build
	@rm -rf preldr/build

$(SUBDIRS):
	@echo "===== Building $(if $(EXTRA_ARGS),(mode: $(EXTRA_ARGS)) ,)$(notdir $@) ====="
	@make -f MakefileApp --no-print-directory\
		-e APP_PATH=$@\
		-e APP_TYPE=$$(basename $@)\
		$(EXTRA_ARGS)

null:
	@make -f MakefileApp --no-print-directory\
		-e APP_PATH=apps/null\
		-e APP_TYPE=null

flash_null:
	@make -f MakefileApp --no-print-directory\
		-e APP_PATH=apps/null\
		-e APP_TYPE=null\
		flash_all

flash_street_sns:
	@make -f MakefileApp --no-print-directory\
		-e APP_PATH=apps/street_sns\
		-e APP_TYPE=street_sns\
		flash_all

gates:
	@make -f MakefileApp --no-print-directory\
		-e APP_PATH=apps/gates\
		-e APP_TYPE=gates

flash_gates:
	@make -f MakefileApp --no-print-directory\
		-e APP_PATH=apps/gates\
		-e APP_TYPE=gates\
		flash_all

program_gates: build/sh_nd_gates/sh_nd_gates_signed.bin
	@canopen_flasher w a $< 80 SH_ND

EXECUTABLE=build/sh_nd_null/sh_nd
# EXECUTABLE=build/sh_nd_street_sns/sh_nd

debug:
	@set _NO_DEBUG_HEAP=1
	@echo "file $(EXECUTABLE)" > .gdbinit
	@echo "set auto-load safe-path /" >> .gdbinit
	@echo "set confirm off" >> .gdbinit
	@echo "target extended-remote :3333" >> .gdbinit
	@arm-none-eabi-gdb -q -x .gdbinit
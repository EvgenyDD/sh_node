ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

__header_offset = 0x110;
__header_size = 0x10;
__dev_serial_preldr_offset = __header_offset + __header_size;

__preldr_start = ORIGIN(PRELDR_FLASH);
__preldr_end = ORIGIN(PRELDR_FLASH) + LENGTH(PRELDR_FLASH);

__ldr_start = ORIGIN(LDR_FLASH);
__ldr_end = ORIGIN(LDR_FLASH) + LENGTH(LDR_FLASH);

__cfg_start = ORIGIN(CFG_FLASH);
__cfg_end = ORIGIN(CFG_FLASH) + LENGTH(CFG_FLASH);

__app_start = ORIGIN(APP_FLASH);
__app_end = ORIGIN(APP_FLASH) + LENGTH(APP_FLASH);


MEMORY
{
PRELDR_FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 3K
LDR_FLASH (rx)    : ORIGIN = 0x08000C00, LENGTH = 24K
CFG_FLASH (rx)    : ORIGIN = 0x08006C00, LENGTH = 1K
APP_FLASH (rx)    : ORIGIN = 0x08007000, LENGTH = 100K

RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
}
#ifndef RET_MEM_H
#define RET_MEM_H

#include <stdint.h>

typedef enum
{
	LOAD_SRC_NONE = 0,

	LOAD_SRC_BOOTLOADER = 0x55,
	LOAD_SRC_APP = 0xAA,
} load_src_t;

enum // byte cell organisation, for the stm32f745VE is 4 kByte
{
	RET_MEM_CELL_LOAD_SRC = 0,
};

void ret_mem_init(void);
load_src_t ret_mem_get_load_src(void);
void ret_mem_set_load_src(load_src_t src);

#endif // RET_MEM_H
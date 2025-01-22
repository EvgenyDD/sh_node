#ifndef RET_MEM_H
#define RET_MEM_H

#include <stdint.h>

typedef enum
{
	LOAD_SRC_NONE = 0,

	LOAD_SRC_BOOTLOADER = 0x55,
	LOAD_SRC_APP = 0xAA,
} load_src_t;

void ret_mem_init(void);
load_src_t ret_mem_get_load_src(void);
void ret_mem_set_load_src(load_src_t src);

void ret_mem_set_rst_cause_ldr(uint32_t src);
void ret_mem_set_rst_cause_app(uint32_t src);
uint32_t ret_mem_get_rst_cause_ldr(void);
uint32_t ret_mem_get_rst_cause_app(void);


#endif // RET_MEM_H
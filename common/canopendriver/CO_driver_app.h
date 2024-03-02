#ifndef CO_DRIVER_H_
#define CO_DRIVER_H_

#include <stdbool.h>
#include "fw_header.h"

void co_od_init_headers(void);

#if FW_TYPE == FW_APP
bool co_is_master_timeout(void);
#endif

#endif // CO_DRIVER_H_
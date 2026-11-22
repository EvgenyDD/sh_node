#ifndef TM1637_H__
#define TM1637_H__

#include <stdint.h>

int tm1637_init(void);

int tm1637_brightness(uint8_t brightness_0_8);
int tm1637_raw(const uint8_t *data);
int tm1637_str(const char *str);
int tm1637_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
int tm1637_clear(void);

#endif // TM1637_H__
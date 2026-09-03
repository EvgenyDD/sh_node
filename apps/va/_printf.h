#ifndef PRINTF_H
#define PRINTF_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int sprintf(char *buffer, const char *format, ...) __attribute__((format(printf, 2, 3)));
int snprintf(char *buffer, size_t count, const char *format, ...) __attribute__((format(printf, 3, 4)));
int vsnprintf(char *buffer, size_t count, const char *format, va_list va);
int fctprintf(void (*out)(char character, void *arg), void *arg, const char *format, ...) __attribute__((format(printf, 3, 4)));

#endif // PRINTF_H

//
// Created by bugliee on 2022/1/18.
//

#ifndef TACRASH_TACC_FMT_H
#define TACRASH_TACC_FMT_H 1

#include <stdint.h>
#include <sys/types.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t tacc_fmt_snprintf(char *buffer, size_t buffer_size, const char *format, ...);
size_t tacc_fmt_vsnprintf(char *buffer, size_t buffer_size, const char *format, va_list args);

#ifdef __cplusplus
}
#endif

#endif //TACRASH_TACC_FMT_H

//
// Created by bugliee on 2022/1/18.
//

#ifndef TACRASH_TACD_UTIL_H
#define TACRASH_TACD_UTIL_H 1

#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t tacd_util_ptrace_read(pid_t pid, uintptr_t addr, void *dst, size_t bytes);
int tacd_util_ptrace_read_fully(pid_t pid, uintptr_t addr, void *dst, size_t bytes);
int tacd_util_ptrace_read_long(pid_t pid, uintptr_t addr, long *value);

int tacd_util_xz_decompress(uint8_t* src, size_t src_size, uint8_t** dst, size_t* dst_size);

#ifdef __cplusplus
}
#endif

#endif //TACRASH_TACD_UTIL_H

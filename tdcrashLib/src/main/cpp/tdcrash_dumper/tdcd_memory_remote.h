//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_MEMORY_REMOTE_H
#define TDCD_MEMORY_REMOTE_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcd_map.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdcd_memory_remote tdcd_memory_remote_t;

int tdcd_memory_remote_create(void **obj, tdcd_map_t *map, pid_t pid);
void tdcd_memory_remote_destroy(void **obj);
size_t tdcd_memory_remote_read(void *obj, uintptr_t addr, void *dst, size_t size);

#ifdef __cplusplus
}
#endif

#endif

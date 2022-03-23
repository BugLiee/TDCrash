//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_MEMORY_REMOTE_H
#define TACD_MEMORY_REMOTE_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tacd_map.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tacd_memory_remote tacd_memory_remote_t;

int tacd_memory_remote_create(void **obj, tacd_map_t *map, pid_t pid);
void tacd_memory_remote_destroy(void **obj);
size_t tacd_memory_remote_read(void *obj, uintptr_t addr, void *dst, size_t size);

#ifdef __cplusplus
}
#endif

#endif

//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_MEMORY_FILE_H
#define TACD_MEMORY_FILE_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tacd_map.h"
#include "tacd_maps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tacd_memory_file tacd_memory_file_t;

int tacd_memory_file_create(void **obj, tacd_memory_t *base, tacd_map_t *map, tacd_maps_t *maps);
void tacd_memory_file_destroy(void **obj);
size_t tacd_memory_file_read(void *obj, uintptr_t addr, void *dst, size_t size);

#ifdef __cplusplus
}
#endif

#endif

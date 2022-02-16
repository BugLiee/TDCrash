//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_MEMORY_FILE_H
#define TDCD_MEMORY_FILE_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcd_map.h"
#include "tdcd_maps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdcd_memory_file tdcd_memory_file_t;

int tdcd_memory_file_create(void **obj, tdcd_memory_t *base, tdcd_map_t *map, tdcd_maps_t *maps);
void tdcd_memory_file_destroy(void **obj);
size_t tdcd_memory_file_read(void *obj, uintptr_t addr, void *dst, size_t size);

#ifdef __cplusplus
}
#endif

#endif

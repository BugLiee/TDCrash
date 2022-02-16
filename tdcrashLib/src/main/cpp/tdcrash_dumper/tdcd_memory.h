//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_MEMORY_H
#define TDCD_MEMORY_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdcd_memory tdcd_memory_t;

typedef struct
{
    void (*destroy)(void **self);
    size_t (*read)(void *self, uintptr_t addr, void *dst, size_t size);
} tdcd_memory_handlers_t;

int tdcd_memory_create(tdcd_memory_t **self, void *map_obj, pid_t pid, void *maps_obj);
int tdcd_memory_create_from_buf(tdcd_memory_t **self, uint8_t *buf, size_t len);
void tdcd_memory_destroy(tdcd_memory_t **self);

size_t tdcd_memory_read(tdcd_memory_t *self, uintptr_t addr, void *dst, size_t size);
int tdcd_memory_read_fully(tdcd_memory_t *self, uintptr_t addr, void* dst, size_t size);
int tdcd_memory_read_string(tdcd_memory_t *self, uintptr_t addr, char *dst, size_t size, size_t max_read);
int tdcd_memory_read_uleb128(tdcd_memory_t *self, uintptr_t addr, uint64_t *dst, size_t *size);
int tdcd_memory_read_sleb128(tdcd_memory_t *self, uintptr_t addr, int64_t *dst, size_t *size);

#ifdef __cplusplus
}
#endif

#endif

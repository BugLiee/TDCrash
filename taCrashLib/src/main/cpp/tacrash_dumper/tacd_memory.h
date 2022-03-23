//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_MEMORY_H
#define TACD_MEMORY_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tacd_memory tacd_memory_t;

typedef struct
{
    void (*destroy)(void **self);
    size_t (*read)(void *self, uintptr_t addr, void *dst, size_t size);
} tacd_memory_handlers_t;

int tacd_memory_create(tacd_memory_t **self, void *map_obj, pid_t pid, void *maps_obj);
int tacd_memory_create_from_buf(tacd_memory_t **self, uint8_t *buf, size_t len);
void tacd_memory_destroy(tacd_memory_t **self);

size_t tacd_memory_read(tacd_memory_t *self, uintptr_t addr, void *dst, size_t size);
int tacd_memory_read_fully(tacd_memory_t *self, uintptr_t addr, void* dst, size_t size);
int tacd_memory_read_string(tacd_memory_t *self, uintptr_t addr, char *dst, size_t size, size_t max_read);
int tacd_memory_read_uleb128(tacd_memory_t *self, uintptr_t addr, uint64_t *dst, size_t *size);
int tacd_memory_read_sleb128(tacd_memory_t *self, uintptr_t addr, int64_t *dst, size_t *size);

#ifdef __cplusplus
}
#endif

#endif

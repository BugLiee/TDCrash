//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_MEMORY_BUF_H
#define TACD_MEMORY_BUF_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tacd_memory_buf tacd_memory_buf_t;

int tacd_memory_buf_create(void **obj, uint8_t *buf, size_t len);
void tacd_memory_buf_destroy(void **obj);
size_t tacd_memory_buf_read(void *obj, uintptr_t addr, void *dst, size_t size);

#ifdef __cplusplus
}
#endif

#endif

//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_MEMORY_BUF_H
#define TDCD_MEMORY_BUF_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdcd_memory_buf tdcd_memory_buf_t;

int tdcd_memory_buf_create(void **obj, uint8_t *buf, size_t len);
void tdcd_memory_buf_destroy(void **obj);
size_t tdcd_memory_buf_read(void *obj, uintptr_t addr, void *dst, size_t size);

#ifdef __cplusplus
}
#endif

#endif

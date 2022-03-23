//
// Created by bugliee on 2022/1/11.
//

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../tacrash/tac_errno.h"
#include "tacc_util.h"
#include "tacd_map.h"
#include "tacd_memory.h"
#include "tacd_memory_buf.h"
#include "tacd_util.h"

struct tacd_memory_buf
{
    uint8_t *buf;
    size_t   len;
};

int tacd_memory_buf_create(void **obj, uint8_t *buf, size_t len)
{
    tacd_memory_buf_t **self = (tacd_memory_buf_t **)obj;
    
    if(NULL == (*self = malloc(sizeof(tacd_memory_buf_t)))) return TAC_ERRNO_NOMEM;
    (*self)->buf = buf;
    (*self)->len = len;

    return 0;
}

void tacd_memory_buf_destroy(void **obj)
{
    tacd_memory_buf_t **self = (tacd_memory_buf_t **)obj;

    free((*self)->buf);
    free(*self);
    *self = NULL;
}

size_t tacd_memory_buf_read(void *obj, uintptr_t addr, void *dst, size_t size)
{
    tacd_memory_buf_t *self = (tacd_memory_buf_t *)obj;

    if((size_t)addr >= self->len) return 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
    size_t read_length = TACC_UTIL_MIN(size, self->len - addr);
#pragma clang diagnostic pop
    
    memcpy(dst, self->buf + addr, read_length);
    return read_length;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
const tacd_memory_handlers_t tacd_memory_buf_handlers = {
    tacd_memory_buf_destroy,
    tacd_memory_buf_read
};
#pragma clang diagnostic pop

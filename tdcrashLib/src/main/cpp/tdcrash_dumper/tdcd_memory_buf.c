//
// Created by bugliee on 2022/1/11.
//

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../tdcrash/tdc_errno.h"
#include "tdcc_util.h"
#include "tdcd_map.h"
#include "tdcd_memory.h"
#include "tdcd_memory_buf.h"
#include "tdcd_util.h"

struct tdcd_memory_buf
{
    uint8_t *buf;
    size_t   len;
};

int tdcd_memory_buf_create(void **obj, uint8_t *buf, size_t len)
{
    tdcd_memory_buf_t **self = (tdcd_memory_buf_t **)obj;
    
    if(NULL == (*self = malloc(sizeof(tdcd_memory_buf_t)))) return TDC_ERRNO_NOMEM;
    (*self)->buf = buf;
    (*self)->len = len;

    return 0;
}

void tdcd_memory_buf_destroy(void **obj)
{
    tdcd_memory_buf_t **self = (tdcd_memory_buf_t **)obj;

    free((*self)->buf);
    free(*self);
    *self = NULL;
}

size_t tdcd_memory_buf_read(void *obj, uintptr_t addr, void *dst, size_t size)
{
    tdcd_memory_buf_t *self = (tdcd_memory_buf_t *)obj;

    if((size_t)addr >= self->len) return 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
    size_t read_length = TDCC_UTIL_MIN(size, self->len - addr);
#pragma clang diagnostic pop
    
    memcpy(dst, self->buf + addr, read_length);
    return read_length;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
const tdcd_memory_handlers_t tdcd_memory_buf_handlers = {
    tdcd_memory_buf_destroy,
    tdcd_memory_buf_read
};
#pragma clang diagnostic pop

//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_DWARF_H
#define TDCD_DWARF_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcd_regs.h"
#include "tdcd_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    TDCD_DWARF_TYPE_DEBUG_FRAME,
    TDCD_DWARF_TYPE_EH_FRAME,
    TDCD_DWARF_TYPE_EH_FRAME_HDR
} tdcd_dwarf_type_t;

typedef struct tdcd_dwarf tdcd_dwarf_t;

int tdcd_dwarf_create(tdcd_dwarf_t **self, tdcd_memory_t *memory, pid_t pid, uintptr_t load_bias, uintptr_t hdr_load_bias,
                     size_t offset, size_t size, tdcd_dwarf_type_t type);

int tdcd_dwarf_step(tdcd_dwarf_t *self, tdcd_regs_t *regs, uintptr_t pc, int *finished);


#ifdef __cplusplus
}
#endif

#endif

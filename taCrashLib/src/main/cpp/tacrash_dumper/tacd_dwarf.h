//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_DWARF_H
#define TACD_DWARF_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tacd_regs.h"
#include "tacd_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    TACD_DWARF_TYPE_DEBUG_FRAME,
    TACD_DWARF_TYPE_EH_FRAME,
    TACD_DWARF_TYPE_EH_FRAME_HDR
} tacd_dwarf_type_t;

typedef struct tacd_dwarf tacd_dwarf_t;

int tacd_dwarf_create(tacd_dwarf_t **self, tacd_memory_t *memory, pid_t pid, uintptr_t load_bias, uintptr_t hdr_load_bias,
                     size_t offset, size_t size, tacd_dwarf_type_t type);

int tacd_dwarf_step(tacd_dwarf_t *self, tacd_regs_t *regs, uintptr_t pc, int *finished);


#ifdef __cplusplus
}
#endif

#endif

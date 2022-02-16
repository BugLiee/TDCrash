//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_MAP_H
#define TDCD_MAP_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcd_elf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDCD_MAP_PORT_DEVICE 0x8000

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct tdcd_map
{
    //base info from /proc/<PID>/maps
    uintptr_t  start;
    uintptr_t  end;
    size_t     offset;
    uint16_t   flags;
    char      *name;

    //ELF
    tdcd_elf_t *elf;
    int        elf_loaded;
    size_t     elf_offset;
    size_t     elf_start_offset;
} tdcd_map_t;
#pragma clang diagnostic pop

int tdcd_map_init(tdcd_map_t *self, uintptr_t start, uintptr_t end, size_t offset,
                 const char * flags, const char *name);
void tdcd_map_uninit(tdcd_map_t *self);

tdcd_elf_t *tdcd_map_get_elf(tdcd_map_t *self, pid_t pid, void *maps_obj);
uintptr_t tdcd_map_get_rel_pc(tdcd_map_t *self, uintptr_t abs_pc, pid_t pid, void *maps_obj);
uintptr_t tdcd_map_get_abs_pc(tdcd_map_t *self, uintptr_t rel_pc, pid_t pid, void *maps_obj);

#ifdef __cplusplus
}
#endif

#endif

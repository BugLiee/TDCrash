//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_MAP_H
#define TACD_MAP_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tacd_elf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TACD_MAP_PORT_DEVICE 0x8000

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct tacd_map
{
    //base info from /proc/<PID>/maps
    uintptr_t  start;
    uintptr_t  end;
    size_t     offset;
    uint16_t   flags;
    char      *name;

    //ELF
    tacd_elf_t *elf;
    int        elf_loaded;
    size_t     elf_offset;
    size_t     elf_start_offset;
} tacd_map_t;
#pragma clang diagnostic pop

int tacd_map_init(tacd_map_t *self, uintptr_t start, uintptr_t end, size_t offset,
                 const char * flags, const char *name);
void tacd_map_uninit(tacd_map_t *self);

tacd_elf_t *tacd_map_get_elf(tacd_map_t *self, pid_t pid, void *maps_obj);
uintptr_t tacd_map_get_rel_pc(tacd_map_t *self, uintptr_t abs_pc, pid_t pid, void *maps_obj);
uintptr_t tacd_map_get_abs_pc(tacd_map_t *self, uintptr_t rel_pc, pid_t pid, void *maps_obj);

#ifdef __cplusplus
}
#endif

#endif

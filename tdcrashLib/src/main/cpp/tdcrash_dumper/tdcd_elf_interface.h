//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_ELF_INTERFACE_H
#define TDCD_ELF_INTERFACE_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcd_memory.h"
#include "tdcd_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdcd_elf_interface tdcd_elf_interface_t;

int tdcd_elf_interface_create(tdcd_elf_interface_t **self, pid_t pid, tdcd_memory_t *memory, uintptr_t *load_bias);

tdcd_elf_interface_t *tdcd_elf_interface_gnu_create(tdcd_elf_interface_t *self);

int tdcd_elf_interface_dwarf_step(tdcd_elf_interface_t *self, uintptr_t step_pc, tdcd_regs_t *regs, int *finished);
#ifdef __arm__
int tdcd_elf_interface_arm_exidx_step(tdcd_elf_interface_t *self, uintptr_t step_pc, tdcd_regs_t *regs, int *finished);
#endif

int tdcd_elf_interface_get_function_info(tdcd_elf_interface_t *self, uintptr_t addr, char **name, size_t *name_offset);
int tdcd_elf_interface_get_symbol_addr(tdcd_elf_interface_t *self, const char *name, uintptr_t *addr);

int tdcd_elf_interface_get_build_id(tdcd_elf_interface_t *self, uint8_t *build_id, size_t build_id_len, size_t *build_id_len_ret);
char *tdcd_elf_interface_get_so_name(tdcd_elf_interface_t *self);

#ifdef __cplusplus
}
#endif

#endif

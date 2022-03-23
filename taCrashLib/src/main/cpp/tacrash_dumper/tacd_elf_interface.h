//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_ELF_INTERFACE_H
#define TACD_ELF_INTERFACE_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tacd_memory.h"
#include "tacd_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tacd_elf_interface tacd_elf_interface_t;

int tacd_elf_interface_create(tacd_elf_interface_t **self, pid_t pid, tacd_memory_t *memory, uintptr_t *load_bias);

tacd_elf_interface_t *tacd_elf_interface_gnu_create(tacd_elf_interface_t *self);

int tacd_elf_interface_dwarf_step(tacd_elf_interface_t *self, uintptr_t step_pc, tacd_regs_t *regs, int *finished);
#ifdef __arm__
int tacd_elf_interface_arm_exidx_step(tacd_elf_interface_t *self, uintptr_t step_pc, tacd_regs_t *regs, int *finished);
#endif

int tacd_elf_interface_get_function_info(tacd_elf_interface_t *self, uintptr_t addr, char **name, size_t *name_offset);
int tacd_elf_interface_get_symbol_addr(tacd_elf_interface_t *self, const char *name, uintptr_t *addr);

int tacd_elf_interface_get_build_id(tacd_elf_interface_t *self, uint8_t *build_id, size_t build_id_len, size_t *build_id_len_ret);
char *tacd_elf_interface_get_so_name(tacd_elf_interface_t *self);

#ifdef __cplusplus
}
#endif

#endif

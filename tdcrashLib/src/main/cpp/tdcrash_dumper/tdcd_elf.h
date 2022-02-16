//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_ELF_H
#define TDCD_ELF_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcd_memory.h"
#include "tdcd_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdcd_elf tdcd_elf_t;

int tdcd_elf_create(tdcd_elf_t **self, pid_t pid, tdcd_memory_t *memory);

int tdcd_elf_step(tdcd_elf_t *self, uintptr_t rel_pc, uintptr_t step_pc, tdcd_regs_t *regs, int *finished, int *sigreturn);

int tdcd_elf_get_function_info(tdcd_elf_t *self, uintptr_t addr, char **name, size_t *name_offset);
int tdcd_elf_get_symbol_addr(tdcd_elf_t *self, const char *name, uintptr_t *addr);

int tdcd_elf_get_build_id(tdcd_elf_t *self, uint8_t *build_id, size_t build_id_len, size_t *build_id_len_ret);
char *tdcd_elf_get_so_name(tdcd_elf_t *self);

uintptr_t tdcd_elf_get_load_bias(tdcd_elf_t *self);
tdcd_memory_t *tdcd_elf_get_memory(tdcd_elf_t *self);

int tdcd_elf_is_valid(tdcd_memory_t *memory);
size_t tdcd_elf_get_max_size(tdcd_memory_t *memory);

#ifdef __cplusplus
}
#endif

#endif

//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_ELF_H
#define TACD_ELF_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tacd_memory.h"
#include "tacd_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tacd_elf tacd_elf_t;

int tacd_elf_create(tacd_elf_t **self, pid_t pid, tacd_memory_t *memory);

int tacd_elf_step(tacd_elf_t *self, uintptr_t rel_pc, uintptr_t step_pc, tacd_regs_t *regs, int *finished, int *sigreturn);

int tacd_elf_get_function_info(tacd_elf_t *self, uintptr_t addr, char **name, size_t *name_offset);
int tacd_elf_get_symbol_addr(tacd_elf_t *self, const char *name, uintptr_t *addr);

int tacd_elf_get_build_id(tacd_elf_t *self, uint8_t *build_id, size_t build_id_len, size_t *build_id_len_ret);
char *tacd_elf_get_so_name(tacd_elf_t *self);

uintptr_t tacd_elf_get_load_bias(tacd_elf_t *self);
tacd_memory_t *tacd_elf_get_memory(tacd_elf_t *self);

int tacd_elf_is_valid(tacd_memory_t *memory);
size_t tacd_elf_get_max_size(tacd_memory_t *memory);

#ifdef __cplusplus
}
#endif

#endif

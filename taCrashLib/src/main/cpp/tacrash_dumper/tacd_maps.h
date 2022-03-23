//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_MAPS_H
#define TACD_MAPS_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tacd_map.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tacd_maps tacd_maps_t;

int tacd_maps_create(tacd_maps_t **self, pid_t pid);
void tacd_maps_destroy(tacd_maps_t **self);

tacd_map_t *tacd_maps_find_map(tacd_maps_t *self, uintptr_t pc);
tacd_map_t *tacd_maps_get_prev_map(tacd_maps_t *self, tacd_map_t *cur_map);

uintptr_t tacd_maps_find_abort_msg(tacd_maps_t *self);

uintptr_t tacd_maps_find_pc(tacd_maps_t *self, const char *pathname, const char *symbol);

int tacd_maps_record(tacd_maps_t *self, int log_fd);

#ifdef __cplusplus
}
#endif

#endif

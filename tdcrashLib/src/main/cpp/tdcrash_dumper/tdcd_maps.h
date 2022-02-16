//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_MAPS_H
#define TDCD_MAPS_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcd_map.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdcd_maps tdcd_maps_t;

int tdcd_maps_create(tdcd_maps_t **self, pid_t pid);
void tdcd_maps_destroy(tdcd_maps_t **self);

tdcd_map_t *tdcd_maps_find_map(tdcd_maps_t *self, uintptr_t pc);
tdcd_map_t *tdcd_maps_get_prev_map(tdcd_maps_t *self, tdcd_map_t *cur_map);

uintptr_t tdcd_maps_find_abort_msg(tdcd_maps_t *self);

uintptr_t tdcd_maps_find_pc(tdcd_maps_t *self, const char *pathname, const char *symbol);

int tdcd_maps_record(tdcd_maps_t *self, int log_fd);

#ifdef __cplusplus
}
#endif

#endif

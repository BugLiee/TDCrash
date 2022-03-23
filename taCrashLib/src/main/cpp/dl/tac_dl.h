//
// Created by bugliee on 2022/1/11.
//

#ifndef TAC_DL_H
#define TAC_DL_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tac_dl tac_dl_t;

tac_dl_t *tac_dl_create(const char *pathname);
void tac_dl_destroy(tac_dl_t **self);

void *tac_dl_sym(tac_dl_t *self, const char *symbol);

#ifdef __cplusplus
}
#endif

#endif

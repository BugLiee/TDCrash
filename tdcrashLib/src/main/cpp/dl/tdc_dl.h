//
// Created by bugliee on 2022/1/11.
//

#ifndef TDC_DL_H
#define TDC_DL_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdc_dl tdc_dl_t;

tdc_dl_t *tdc_dl_create(const char *pathname);
void tdc_dl_destroy(tdc_dl_t **self);

void *tdc_dl_sym(tdc_dl_t *self, const char *symbol);

#ifdef __cplusplus
}
#endif

#endif

//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCRASH_TDC_UTIL_H
#define TDCRASH_TDC_UTIL_H 1

#ifdef __cplusplus
extern "C" {
#endif

char *tdc_util_strdupcat(const char *s1, const char *s2);
int tdc_util_mkdirs(const char *dir);
void tdc_util_get_kernel_version(char *buf, size_t len);

#ifdef __cplusplus
}
#endif


#endif //TDCRASH_TDC_UTIL_H

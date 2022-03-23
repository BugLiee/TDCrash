//
// Created by bugliee on 2022/1/11.
//

#ifndef TACRASH_TAC_UTIL_H
#define TACRASH_TAC_UTIL_H 1

#ifdef __cplusplus
extern "C" {
#endif

char *tac_util_strdupcat(const char *s1, const char *s2);
int tac_util_mkdirs(const char *dir);
void tac_util_get_kernel_version(char *buf, size_t len);

#ifdef __cplusplus
}
#endif


#endif //TACRASH_TAC_UTIL_H

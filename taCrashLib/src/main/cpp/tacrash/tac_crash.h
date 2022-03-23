//
// Created by bugliee on 2022/1/11.
//

#ifndef TACRASH_TAC_CRASH_H
#define TACRASH_TAC_CRASH_H 1

#include <stdint.h>
#include <sys/types.h>
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

int tac_crash_init(JNIEnv *env, int rethrow);

#ifdef __cplusplus
}
#endif

#endif //TACRASH_TAC_CRASH_H

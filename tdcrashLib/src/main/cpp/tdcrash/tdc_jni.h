//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCRASH_TDC_JNI_H
#define TDCRASH_TDC_JNI_H 1

#include <stdint.h>
#include <sys/types.h>
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TDC_JNI_IGNORE_PENDING_EXCEPTION()                 \
    do {                                                       \
        if((*env)->ExceptionCheck(env))                        \
        {                                                      \
            (*env)->ExceptionClear(env);                       \
        }                                                      \
    } while(0)

#define TDC_JNI_CHECK_PENDING_EXCEPTION(label)             \
    do {                                                       \
        if((*env)->ExceptionCheck(env))                        \
        {                                                      \
            (*env)->ExceptionClear(env);                       \
            goto label;                                        \
        }                                                      \
    } while(0)

#define TDC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(v, label) \
    do {                                                       \
        TDC_JNI_CHECK_PENDING_EXCEPTION(label);            \
        if(NULL == (v)) goto label;                            \
    } while(0)

#define TDC_JNI_VERSION    JNI_VERSION_1_6
#define TDC_JNI_CLASS_NAME "com/thinkingdata/tdcrash/NativeHandler"

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDC_JNI_H

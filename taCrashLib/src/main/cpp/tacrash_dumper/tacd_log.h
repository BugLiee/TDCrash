//
// Created by bugliee on 2022/1/11.
//

#ifndef TACD_LOG_H
#define TACD_LOG_H 1

#include <stdarg.h>
#include <android/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TACD_LOG_PRIO ANDROID_LOG_WARN

#define TACD_LOG_TAG "tacrash_dumper"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define TACD_LOG_DEBUG(fmt, ...) do{if(TACD_LOG_PRIO <= ANDROID_LOG_DEBUG) __android_log_print(ANDROID_LOG_DEBUG, TACD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#define TACD_LOG_INFO(fmt, ...)  do{if(TACD_LOG_PRIO <= ANDROID_LOG_INFO)  __android_log_print(ANDROID_LOG_INFO,  TACD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#define TACD_LOG_WARN(fmt, ...)  do{if(TACD_LOG_PRIO <= ANDROID_LOG_WARN)  __android_log_print(ANDROID_LOG_WARN,  TACD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#define TACD_LOG_ERROR(fmt, ...) do{if(TACD_LOG_PRIO <= ANDROID_LOG_ERROR) __android_log_print(ANDROID_LOG_ERROR, TACD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#pragma clang diagnostic pop

//debug-log flags for modules
#define TACD_CORE_DEBUG          0
#define TACD_THREAD_DEBUG        0
#define TACD_ELF_DEBUG           0
#define TACD_ELF_INTERFACE_DEBUG 0
#define TACD_FRAMES_DEBUG        0
#define TACD_DWARF_DEBUG         0
#define TACD_ARM_EXIDX_DEBUG     0

#ifdef __cplusplus
}
#endif

#endif

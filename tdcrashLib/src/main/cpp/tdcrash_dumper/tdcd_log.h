//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_LOG_H
#define TDCD_LOG_H 1

#include <stdarg.h>
#include <android/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TDCD_LOG_PRIO ANDROID_LOG_WARN

#define TDCD_LOG_TAG "tdcrash_dumper"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define TDCD_LOG_DEBUG(fmt, ...) do{if(TDCD_LOG_PRIO <= ANDROID_LOG_DEBUG) __android_log_print(ANDROID_LOG_DEBUG, TDCD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#define TDCD_LOG_INFO(fmt, ...)  do{if(TDCD_LOG_PRIO <= ANDROID_LOG_INFO)  __android_log_print(ANDROID_LOG_INFO,  TDCD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#define TDCD_LOG_WARN(fmt, ...)  do{if(TDCD_LOG_PRIO <= ANDROID_LOG_WARN)  __android_log_print(ANDROID_LOG_WARN,  TDCD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#define TDCD_LOG_ERROR(fmt, ...) do{if(TDCD_LOG_PRIO <= ANDROID_LOG_ERROR) __android_log_print(ANDROID_LOG_ERROR, TDCD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#pragma clang diagnostic pop

//debug-log flags for modules
#define TDCD_CORE_DEBUG          0
#define TDCD_THREAD_DEBUG        0
#define TDCD_ELF_DEBUG           0
#define TDCD_ELF_INTERFACE_DEBUG 0
#define TDCD_FRAMES_DEBUG        0
#define TDCD_DWARF_DEBUG         0
#define TDCD_ARM_EXIDX_DEBUG     0

#ifdef __cplusplus
}
#endif

#endif

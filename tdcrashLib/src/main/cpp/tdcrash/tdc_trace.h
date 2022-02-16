//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCRASH_TDC_TRACE_H
#define TDCRASH_TDC_TRACE_H 1

#include <stdint.h>
#include <setjmp.h>
#include <sys/types.h>
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TDC_TRACE_DUMP_NOT_START = 0,
    TDC_TRACE_DUMP_ON_GOING,
    TDC_TRACE_DUMP_ART_CRASH,
    TDC_TRACE_DUMP_END
} tdc_trace_dump_status_t;

extern tdc_trace_dump_status_t tdc_trace_dump_status;
extern sigjmp_buf      jmpenv;

int tdc_trace_init(JNIEnv *env,
                  int rethrow,
                  unsigned int logcat_system_lines,
                  unsigned int logcat_events_lines,
                  unsigned int logcat_main_lines,
                  int dump_fds,
                  int dump_network_info);

#ifdef __cplusplus
}
#endif


#endif //TDCRASH_TDC_TRACE_H

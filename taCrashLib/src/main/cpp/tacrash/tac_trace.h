//
// Created by bugliee on 2022/1/11.
//

#ifndef TACRASH_TAC_TRACE_H
#define TACRASH_TAC_TRACE_H 1

#include <stdint.h>
#include <setjmp.h>
#include <sys/types.h>
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TAC_TRACE_DUMP_NOT_START = 0,
    TAC_TRACE_DUMP_ON_GOING,
    TAC_TRACE_DUMP_ART_CRASH,
    TAC_TRACE_DUMP_END
} tac_trace_dump_status_t;

extern tac_trace_dump_status_t tac_trace_dump_status;
extern sigjmp_buf      jmpenv;

int tac_trace_init(JNIEnv *env, int rethrow);

#ifdef __cplusplus
}
#endif


#endif //TACRASH_TAC_TRACE_H

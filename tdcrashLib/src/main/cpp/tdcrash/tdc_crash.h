//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCRASH_TDC_CRASH_H
#define TDCRASH_TDC_CRASH_H 1

#include <stdint.h>
#include <sys/types.h>
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

int tdc_crash_init(JNIEnv *env,
                  int rethrow,
                  unsigned int logcat_system_lines,
                  unsigned int logcat_events_lines,
                  unsigned int logcat_main_lines,
                  int dump_elf_hash,
                  int dump_map,
                  int dump_fds,
                  int dump_network_info,
                  int dump_all_threads,
                  unsigned int dump_all_threads_count_max,
                  const char **dump_all_threads_whitelist,
                  size_t dump_all_threads_whitelist_len);

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDC_CRASH_H

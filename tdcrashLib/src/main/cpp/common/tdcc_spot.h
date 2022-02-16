//
// Created by bugliee on 2022/1/18.
//

#ifndef TDCRASH_TDCC_SPOT_H
#define TDCRASH_TDCC_SPOT_H 1

#include <stdint.h>
#include <sys/types.h>
#include <ucontext.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

typedef struct
{
    //set when crashed
    pid_t        crash_tid;
    siginfo_t    siginfo;
    ucontext_t   ucontext;
    uint64_t     crash_time;

    //set when inited
    int          api_level;
    pid_t        crash_pid;
    uint64_t     start_time;
    long         time_zone;
    unsigned int logcat_system_lines;
    unsigned int logcat_events_lines;
    unsigned int logcat_main_lines;
    int          dump_elf_hash;
    int          dump_map;
    int          dump_fds;
    int          dump_network_info;
    int          dump_all_threads;
    unsigned int dump_all_threads_count_max;

    //set when crashed (content lengths after this struct)
    size_t       log_pathname_len;

    //set when inited (content lengths after this struct)
    size_t       os_version_len;
    size_t       kernel_version_len;
    size_t       abi_list_len;
    size_t       manufacturer_len;
    size_t       brand_len;
    size_t       model_len;
    size_t       build_fingerprint_len;
    size_t       app_id_len;
    size_t       app_version_len;
    size_t       dump_all_threads_whitelist_len;
} tdcc_spot_t;

#pragma clang diagnostic pop

#ifdef __cplusplus
}
#endif


#endif //TDCRASH_TDCC_SPOT_H

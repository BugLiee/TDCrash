//
// Created by bugliee on 2022/1/18.
//

#ifndef TACRASH_TACC_SPOT_H
#define TACRASH_TACC_SPOT_H 1

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
} tacc_spot_t;

#pragma clang diagnostic pop

#ifdef __cplusplus
}
#endif


#endif //TACRASH_TACC_SPOT_H

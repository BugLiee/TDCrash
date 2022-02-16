//
// Created by bugliee on 2022/1/11.
//

#ifndef TDCD_SYS_H
#define TDCD_SYS_H 1

#include <stdint.h>
#include <sys/types.h>
#include "tdcc_util.h"

#ifdef __cplusplus
extern "C" {
#endif

int tdcd_sys_record(int fd,
                   long time_zone,
                   uint64_t start_time,
                   uint64_t crash_time,
                   const char *app_id,
                   const char *app_version,
                   int api_level,
                   const char *os_version,
                   const char *kernel_version,
                   const char *abi_list,
                   const char *manufacturer,
                   const char *brand,
                   const char *model,
                   const char *build_fingerprint);

#ifdef __cplusplus
}
#endif

#endif

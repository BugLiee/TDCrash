//
// Created by bugliee on 2022/1/11.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "tdcc_util.h"
#include "tdcd_sys.h"

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
                   const char *build_fingerprint)
{
    char buf[1024];
    tdcc_util_get_dump_header(buf, sizeof(buf),
                             TDCC_UTIL_CRASH_TYPE_NATIVE,
                             time_zone,
                             start_time,
                             crash_time,
                             app_id,
                             app_version,
                             api_level,
                             os_version,
                             kernel_version,
                             abi_list,
                             manufacturer,
                             brand,
                             model,
                             build_fingerprint);
    return tdcc_util_write_str(fd, buf);
}

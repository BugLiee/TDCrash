//
// Created by bugliee on 2022/1/18.
//

#ifndef TDCRASH_TDCC_MEMINFO_H
#define TDCRASH_TDCC_MEMINFO_H 1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int tdcc_meminfo_record(int log_fd, pid_t pid);

#ifdef __cplusplus
}
#endif

#endif //TDCRASH_TDCC_MEMINFO_H

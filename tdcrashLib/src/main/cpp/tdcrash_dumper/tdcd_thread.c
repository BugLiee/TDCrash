//
// Created by bugliee on 2022/1/11.
//

#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <ucontext.h>
#include <dirent.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <linux/elf.h>
#include "../tdcrash/tdc_errno.h"
#include "tdcc_util.h"
#include "tdcd_thread.h"
#include "tdcd_frames.h"
#include "tdcd_regs.h"
#include "tdcd_util.h"
#include "tdcd_log.h"

void tdcd_thread_init(tdcd_thread_t *self, pid_t pid, pid_t tid)
{
    self->status = TDCD_THREAD_STATUS_OK;
    self->pid    = pid;
    self->tid    = tid;
    self->tname  = NULL;
    self->frames = NULL;
    memset(&(self->regs), 0, sizeof(self->regs));
}

void tdcd_thread_suspend(tdcd_thread_t *self)
{
    if(0 != ptrace(PTRACE_ATTACH, self->tid, NULL, NULL))
    {
#if TDCD_THREAD_DEBUG
        TDCD_LOG_WARN("THREAD: ptrace ATTACH failed, errno=%d", errno);
#endif
        self->status = TDCD_THREAD_STATUS_ATTACH;
        return;
    }

    errno = 0;
    while(waitpid(self->tid, NULL, __WALL) < 0)
    {
        if(EINTR != errno)
        {
            ptrace(PTRACE_DETACH, self->tid, NULL, NULL);
#if TDCD_THREAD_DEBUG
            TDCD_LOG_ERROR("THREAD: waitpid for ptrace ATTACH failed, errno=%d", errno);
#endif
            self->status = TDCD_THREAD_STATUS_ATTACH_WAIT;
            return;
        }
        errno = 0;
    }
}

void tdcd_thread_resume(tdcd_thread_t *self)
{
    ptrace(PTRACE_DETACH, self->tid, NULL, NULL);
}

void tdcd_thread_load_info(tdcd_thread_t *self)
{
    char buf[64] = "\0";
    
    tdcc_util_get_thread_name(self->tid, buf, sizeof(buf));
    if(NULL == (self->tname = strdup(buf))) self->tname = "unknown";
}

void tdcd_thread_load_regs(tdcd_thread_t *self)
{
    uintptr_t regs[64]; //big enough for all architectures
    size_t    regs_len;
    
#ifdef PTRACE_GETREGS
    if(0 != ptrace(PTRACE_GETREGS, self->tid, NULL, &regs))
    {
        TDCD_LOG_ERROR("THREAD: ptrace GETREGS failed, errno=%d", errno);
        self->status = TDCD_THREAD_STATUS_REGS;
        return;
    }
    regs_len = TDCD_REGS_USER_NUM;
#else
    struct iovec iovec;
    iovec.iov_base = &regs;
    iovec.iov_len = sizeof(regs);
    if(0 != ptrace(PTRACE_GETREGSET, self->tid, (void *)NT_PRSTATUS, &iovec))
    {
        TDCD_LOG_ERROR("THREAD: ptrace GETREGSET failed, errno=%d", errno);
        self->status = TDCD_THREAD_STATUS_REGS;
        return;
    }
    regs_len = iovec.iov_len / sizeof(uintptr_t);
#endif
    tdcd_regs_load_from_ptregs(&(self->regs), regs, regs_len);
}

void tdcd_thread_load_regs_from_ucontext(tdcd_thread_t *self, ucontext_t *uc)
{
    tdcd_regs_load_from_ucontext(&(self->regs), uc);
}

int tdcd_thread_load_frames(tdcd_thread_t *self, tdcd_maps_t *maps)
{
#if TDCD_THREAD_DEBUG
    TDCD_LOG_DEBUG("THREAD: load frames, tid=%d, tname=%s", self->tid, self->tname);
#endif

    if(TDCD_THREAD_STATUS_OK != self->status) return TDC_ERRNO_STATE; //do NOT ignore

    return tdcd_frames_create(&(self->frames), &(self->regs), maps, self->pid);
}

int tdcd_thread_record_info(tdcd_thread_t *self, int log_fd, const char *pname)
{
    return tdcc_util_write_format(log_fd, "pid: %d, tid: %d, name: %s  >>> %s <<<\n",
                                 self->pid, self->tid, self->tname, pname);
}

int tdcd_thread_record_regs(tdcd_thread_t *self, int log_fd)
{
    if(TDCD_THREAD_STATUS_OK != self->status) return 0; //ignore
    
    return tdcd_regs_record(&(self->regs), log_fd);
}

int tdcd_thread_record_backtrace(tdcd_thread_t *self, int log_fd)
{
    if(TDCD_THREAD_STATUS_OK != self->status) return 0; //ignore

    return tdcd_frames_record_backtrace(self->frames, log_fd);
}

int tdcd_thread_record_buildid(tdcd_thread_t *self, int log_fd, int dump_elf_hash, uintptr_t fault_addr)
{
    if(TDCD_THREAD_STATUS_OK != self->status) return 0; //ignore

    return tdcd_frames_record_buildid(self->frames, log_fd, dump_elf_hash, fault_addr);
}

int tdcd_thread_record_stack(tdcd_thread_t *self, int log_fd)
{
    if(TDCD_THREAD_STATUS_OK != self->status) return 0; //ignore
    
    return tdcd_frames_record_stack(self->frames, log_fd);
}

#define TDCD_THREAD_MEMORY_BYTES_TO_DUMP 256
#define TDCD_THREAD_MEMORY_BYTES_PER_LINE 16

static int tdcd_thread_record_memory_by_addr(tdcd_thread_t *self, int log_fd,
                                            const char *label, uintptr_t addr)
{
    int r;
    
    // Align the address to sizeof(long) and start 32 bytes before the address.
    addr &= ~(sizeof(long) - 1);
    if (addr >= 4128) addr -= 32;

    // Don't bother if the address looks too low, or looks too high.
    if (addr < 4096 ||
#if defined(__LP64__)
        addr > 0x4000000000000000UL - TDCD_THREAD_MEMORY_BYTES_TO_DUMP) {
#else
        addr > 0xffff0000 - TDCD_THREAD_MEMORY_BYTES_TO_DUMP) {
#endif
        return 0; //not an error
    }

    if(0 != (r = tdcc_util_write_format(log_fd, "memory near %s:\n", label))) return r;
    
    // Dump 256 bytes
    uintptr_t data[TDCD_THREAD_MEMORY_BYTES_TO_DUMP/sizeof(uintptr_t)];
    memset(data, 0, TDCD_THREAD_MEMORY_BYTES_TO_DUMP);
    size_t bytes = tdcd_util_ptrace_read(self->pid, addr, data, sizeof(data));
    if (bytes % sizeof(uintptr_t) != 0)
        bytes &= ~(sizeof(uintptr_t) - 1);
    
    uint64_t start = 0;
    int skip_2nd_read = 0;
    if(bytes == 0)
    {
        // In this case, we might want to try another read at the beginning of
        // the next page only if it's within the amount of memory we would have
        // read.
        size_t page_size = (size_t)sysconf(_SC_PAGE_SIZE);
        start = ((addr + (page_size - 1)) & ~(page_size - 1)) - addr;
        if (start == 0 || start >= TDCD_THREAD_MEMORY_BYTES_TO_DUMP)
            skip_2nd_read = 1;
    }

    if (bytes < TDCD_THREAD_MEMORY_BYTES_TO_DUMP && !skip_2nd_read)
    {
        // Try to do one more read. This could happen if a read crosses a map,
        // but the maps do not have any break between them. Or it could happen
        // if reading from an unreadable map, but the read would cross back
        // into a readable map. Only requires one extra read because a map has
        // to contain at least one page, and the total number of bytes to dump
        // is smaller than a page.
        size_t bytes2 = tdcd_util_ptrace_read(self->pid, (uintptr_t)(addr + start + bytes), (uint8_t *)(data) + bytes,
                                             (size_t)(sizeof(data) - bytes - start));
        bytes += bytes2;
        if(bytes2 > 0 && bytes % sizeof(uintptr_t) != 0)
            bytes &= ~(sizeof(uintptr_t) - 1);
    }
    uintptr_t *data_ptr = data;
    uint8_t *ptr;
    size_t current = 0;
    size_t total_bytes = (size_t)(start + bytes);
    size_t i, j, k;
    char ascii[TDCD_THREAD_MEMORY_BYTES_PER_LINE + 1];
    size_t ascii_idx = 0;
    char line[128];
    size_t line_len = 0;
    for(j = 0; j < TDCD_THREAD_MEMORY_BYTES_TO_DUMP / TDCD_THREAD_MEMORY_BYTES_PER_LINE; j++)
    {
        ascii_idx = 0;
        
        line_len = (size_t)snprintf(line, sizeof(line), "    %0"TDCC_UTIL_FMT_ADDR, addr);
        addr += TDCD_THREAD_MEMORY_BYTES_PER_LINE;

        for(i = 0; i < TDCD_THREAD_MEMORY_BYTES_PER_LINE / sizeof(uintptr_t); i++)
        {
            if(current >= start && current + sizeof(uintptr_t) <= total_bytes)
            {
                line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len, " %0"TDCC_UTIL_FMT_ADDR, *data_ptr);
                
                // Fill out the ascii string from the data.
                ptr = (uint8_t *)data_ptr;
                for(k = 0; k < sizeof(uintptr_t); k++, ptr++)
                {
                    if(*ptr >= 0x20 && *ptr < 0x7f)
                    {
                        ascii[ascii_idx++] = (char)(*ptr);
                    }
                    else
                    {
                        ascii[ascii_idx++] = '.';
                    }
                }
                data_ptr++;
            }
            else
            {
                line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len, " ");
                for(k = 0; k < sizeof(uintptr_t) * 2; k++)
                    line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len, "-");
                for(k = 0; k < sizeof(uintptr_t); k++)
                    ascii[ascii_idx++] = '.';
            }
            current += sizeof(uintptr_t);
        }
        ascii[ascii_idx] = '\0';

        if(0 != (r = tdcc_util_write_format(log_fd, "%s  %s\n", line, ascii))) return r;
    }

    if(0 != (r = tdcc_util_write_str(log_fd, "\n"))) return r;

    return 0;
}

int tdcd_thread_record_memory(tdcd_thread_t *self, int log_fd)
{
    tdcd_regs_label_t *labels;
    size_t            labels_count;
    size_t            i;
    int               r;

    if(TDCD_THREAD_STATUS_OK != self->status) return 0; //ignore

    tdcd_regs_get_labels(&labels, &labels_count);
    
    for(i = 0; i < labels_count; i++)
        if(0 != (r = tdcd_thread_record_memory_by_addr(self, log_fd, labels[i].name,
                                                      (uintptr_t)(self->regs.r[labels[i].idx])))) return r;

    return 0;
}

//
// Created by bugliee on 2022/1/18.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>
#include <ucontext.h>
#include <unwind.h>
#include <dlfcn.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <android/log.h>
#include "tdcc_unwind_clang.h"
#include "../tdcrash/tdc_errno.h"
#include "tdcc_util.h"
#include "tdcc_fmt.h"
#include "tdcc_libc_support.h"

#define MAX_FRAMES 64

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

typedef struct
{
    size_t      frame_num;
    ucontext_t *uc;
    char       *buf;
    size_t      buf_len;
    size_t      buf_used;
    uintptr_t   prev_pc;
    uintptr_t   prev_sp;
    uintptr_t   sig_pc;
    uintptr_t   sig_lr;
    int         found_sig_pc;
} tdcc_unwind_clang_t;

#pragma clang diagnostic pop

static int tdcc_unwind_clang_record_frame(tdcc_unwind_clang_t *self, uintptr_t pc)
{
    Dl_info info;
    size_t  len;

    //append line for current frame
    if(0 == dladdr((void *)pc, &info) || (uintptr_t)info.dli_fbase > pc)
    {
        len = tdcc_fmt_snprintf(self->buf + self->buf_used, self->buf_len - self->buf_used,
                               "    #%02zu pc %0"TDCC_UTIL_FMT_ADDR"  <unknown>\n",
                self->frame_num, pc);
    }
    else
    {
        if(NULL == info.dli_fname || '\0' == info.dli_fname[0])
        {
            len = tdcc_fmt_snprintf(self->buf + self->buf_used, self->buf_len - self->buf_used,
                                   "    #%02zu pc %0"TDCC_UTIL_FMT_ADDR"  <anonymous:%"TDCC_UTIL_FMT_ADDR">\n",
                    self->frame_num, pc - (uintptr_t)info.dli_fbase, (uintptr_t)info.dli_fbase);
        }
        else
        {
            if(NULL == info.dli_sname || '\0' == info.dli_sname[0])
            {
                len = tdcc_fmt_snprintf(self->buf + self->buf_used, self->buf_len - self->buf_used,
                                       "    #%02zu pc %0"TDCC_UTIL_FMT_ADDR"  %s\n",
                        self->frame_num, pc - (uintptr_t)info.dli_fbase, info.dli_fname);
            }
            else
            {
                if(0 == (uintptr_t)info.dli_saddr || (uintptr_t)info.dli_saddr > pc)
                {
                    len = tdcc_fmt_snprintf(self->buf + self->buf_used, self->buf_len - self->buf_used,
                                           "    #%02zu pc %0"TDCC_UTIL_FMT_ADDR"  %s (%s)\n",
                            self->frame_num, pc - (uintptr_t)info.dli_fbase, info.dli_fname,
                            info.dli_sname);
                }
                else
                {
                    len = tdcc_fmt_snprintf(self->buf + self->buf_used, self->buf_len - self->buf_used,
                                           "    #%02zu pc %0"TDCC_UTIL_FMT_ADDR"  %s (%s+%"PRIuPTR")\n",
                            self->frame_num, pc - (uintptr_t)info.dli_fbase, info.dli_fname,
                            info.dli_sname, pc - (uintptr_t)info.dli_saddr);
                }
            }
        }
    }

    //truncated?
    if(len >= self->buf_len - self->buf_used)
    {
        self->buf[self->buf_len - 2] = '\n';
        self->buf[self->buf_len - 1] = '\0';
        len = self->buf_len - self->buf_used - 1;
    }

    //check remaining space length in the buffer
    self->buf_used += len;
    if(self->buf_len - self->buf_used < 20) return TDC_ERRNO_NOSPACE;

    //check unwinding limit
    self->frame_num++;
    if(self->frame_num >= MAX_FRAMES) return TDC_ERRNO_RANGE;

    return 0;
}

static _Unwind_Reason_Code tdcc_unwind_clang_callback(struct _Unwind_Context* unw_ctx, void* arg)
{
    tdcc_unwind_clang_t *self = (tdcc_unwind_clang_t *)arg;
    uintptr_t pc = _Unwind_GetIP(unw_ctx);
    uintptr_t sp = _Unwind_GetCFA(unw_ctx);

    if(0 == self->found_sig_pc)
    {
        if((self->sig_pc >= sizeof(uintptr_t) && pc <= self->sig_pc + sizeof(uintptr_t) && pc >= self->sig_pc - sizeof(uintptr_t)) ||
           (self->sig_lr >= sizeof(uintptr_t) && pc <= self->sig_lr + sizeof(uintptr_t) && pc >= self->sig_lr - sizeof(uintptr_t)))
        {
            self->found_sig_pc = 1;
        }
        else
        {
            return _URC_NO_REASON; //skip and continue
        }
    }

    if(self->frame_num > 0 && pc == self->prev_pc && sp == self->prev_sp)
        return _URC_END_OF_STACK; //stop

    if(0 != tdcc_unwind_clang_record_frame(self, pc))
        return _URC_END_OF_STACK; //stop

    self->prev_pc = pc;
    self->prev_sp = sp;

    return _URC_NO_REASON; //continue
}

size_t tdcc_unwind_clang_record(ucontext_t *uc, char *buf, size_t buf_len)
{
    tdcc_unwind_clang_t self;

    tdcc_libc_support_memset(&self, 0, sizeof(tdcc_unwind_clang_t));
    self.uc      = uc;
    self.buf     = buf;
    self.buf_len = buf_len;

    //Trying to get LR for x86 and x86_64 on local unwind is usually
    //leads to access unmapped memory, which will crash the process immediately.
#if defined(__arm__)
    self.sig_pc = (uintptr_t)uc->uc_mcontext.arm_pc;
    self.sig_lr = (uintptr_t)uc->uc_mcontext.arm_lr;
#elif defined(__aarch64__)
    self.sig_pc = (uintptr_t)uc->uc_mcontext.pc;
    self.sig_lr = (uintptr_t)uc->uc_mcontext.regs[30];
#elif defined(__i386__)
    self.sig_pc = (uintptr_t)uc->uc_mcontext.gregs[REG_EIP];
#elif defined(__x86_64__)
    self.sig_pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
#endif

    _Unwind_Backtrace(tdcc_unwind_clang_callback, &self);

    if(0 == self.buf_used)
        tdcc_unwind_clang_record_frame(&self, self.sig_pc);

    return self.buf_used;
}

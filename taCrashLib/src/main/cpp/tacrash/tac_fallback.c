//
// Created by bugliee on 2022/1/11.
//

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#define _GNU_SOURCE
#pragma clang diagnostic pop

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>
#include <ucontext.h>
#include <unwind.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "../tacrash/tac_errno.h"
#include "tacc_util.h"
#include "tacc_unwind.h"
#include "tacc_fmt.h"
#include "tacc_version.h"
#include "tac_fallback.h"
#include "tac_common.h"
#include "tac_util.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

static size_t tac_fallback_get_process_thread(char *buf, size_t len, pid_t tid)
{
    char  tname[64];

    tacc_util_get_thread_name(tid, tname, sizeof(tname));

    return tacc_fmt_snprintf(buf, len, "pid: %d, tid: %d, name: %s  >>> %s <<<\n",
                            tac_common_process_id, tid, tname, tac_common_process_name);
}

static size_t tac_fallback_get_signal(char *buf, size_t len, siginfo_t *si)
{
    //fault addr
    char addr_desc[64];
    if(tacc_util_signal_has_si_addr(si))
        tacc_fmt_snprintf(addr_desc, sizeof(addr_desc), "%p", si->si_addr);
    else
        tacc_fmt_snprintf(addr_desc, sizeof(addr_desc), "--------");

    //from
    char sender_desc[64] = "";
    if(tacc_util_signal_has_sender(si, tac_common_process_id))
        tacc_fmt_snprintf(sender_desc, sizeof(sender_desc), " from pid %d, uid %d", si->si_pid, si->si_uid);

    return tacc_fmt_snprintf(buf, len, "signal %d (%s), code %d (%s%s), fault addr %s\n",
                            si->si_signo, tacc_util_get_signame(si),
                            si->si_code, tacc_util_get_sigcodename(si), sender_desc, addr_desc);
}

static size_t tac_fallback_get_regs(char *buf, size_t len, ucontext_t *uc)
{
#if defined(__arm__)
    return tacc_fmt_snprintf(buf, len,
                            "    r0  %08x  r1  %08x  r2  %08x  r3  %08x\n"
                            "    r4  %08x  r5  %08x  r6  %08x  r7  %08x\n"
                            "    r8  %08x  r9  %08x  r10 %08x  r11 %08x\n"
                            "    ip  %08x  sp  %08x  lr  %08x  pc  %08x\n\n",
                            uc->uc_mcontext.arm_r0,
                            uc->uc_mcontext.arm_r1,
                            uc->uc_mcontext.arm_r2,
                            uc->uc_mcontext.arm_r3,
                            uc->uc_mcontext.arm_r4,
                            uc->uc_mcontext.arm_r5,
                            uc->uc_mcontext.arm_r6,
                            uc->uc_mcontext.arm_r7,
                            uc->uc_mcontext.arm_r8,
                            uc->uc_mcontext.arm_r9,
                            uc->uc_mcontext.arm_r10,
                            uc->uc_mcontext.arm_fp,
                            uc->uc_mcontext.arm_ip,
                            uc->uc_mcontext.arm_sp,
                            uc->uc_mcontext.arm_lr,
                            uc->uc_mcontext.arm_pc);
#elif defined(__aarch64__)
    return tacc_fmt_snprintf(buf, len,
                            "    x0  %016lx  x1  %016lx  x2  %016lx  x3  %016lx\n"
                            "    x4  %016lx  x5  %016lx  x6  %016lx  x7  %016lx\n"
                            "    x8  %016lx  x9  %016lx  x10 %016lx  x11 %016lx\n"
                            "    x12 %016lx  x13 %016lx  x14 %016lx  x15 %016lx\n"
                            "    x16 %016lx  x17 %016lx  x18 %016lx  x19 %016lx\n"
                            "    x20 %016lx  x21 %016lx  x22 %016lx  x23 %016lx\n"
                            "    x24 %016lx  x25 %016lx  x26 %016lx  x27 %016lx\n"
                            "    x28 %016lx  x29 %016lx\n"
                            "    sp  %016lx  lr  %016lx  pc  %016lx\n\n",
                            uc->uc_mcontext.regs[0],
                            uc->uc_mcontext.regs[1],
                            uc->uc_mcontext.regs[2],
                            uc->uc_mcontext.regs[3],
                            uc->uc_mcontext.regs[4],
                            uc->uc_mcontext.regs[5],
                            uc->uc_mcontext.regs[6],
                            uc->uc_mcontext.regs[7],
                            uc->uc_mcontext.regs[8],
                            uc->uc_mcontext.regs[9],
                            uc->uc_mcontext.regs[10],
                            uc->uc_mcontext.regs[11],
                            uc->uc_mcontext.regs[12],
                            uc->uc_mcontext.regs[13],
                            uc->uc_mcontext.regs[14],
                            uc->uc_mcontext.regs[15],
                            uc->uc_mcontext.regs[16],
                            uc->uc_mcontext.regs[17],
                            uc->uc_mcontext.regs[18],
                            uc->uc_mcontext.regs[19],
                            uc->uc_mcontext.regs[20],
                            uc->uc_mcontext.regs[21],
                            uc->uc_mcontext.regs[22],
                            uc->uc_mcontext.regs[23],
                            uc->uc_mcontext.regs[24],
                            uc->uc_mcontext.regs[25],
                            uc->uc_mcontext.regs[26],
                            uc->uc_mcontext.regs[27],
                            uc->uc_mcontext.regs[28],
                            uc->uc_mcontext.regs[29],
                            uc->uc_mcontext.sp,
                            uc->uc_mcontext.regs[30], //lr
                            uc->uc_mcontext.pc);
#elif defined(__i386__)
    return tacc_fmt_snprintf(buf, len,
                            "    eax %08x  ebx %08x  ecx %08x  edx %08x\n"
                            "    edi %08x  esi %08x\n"
                            "    ebp %08x  esp %08x  eip %08x\n\n",
                            uc->uc_mcontext.gregs[REG_EAX],
                            uc->uc_mcontext.gregs[REG_EBX],
                            uc->uc_mcontext.gregs[REG_ECX],
                            uc->uc_mcontext.gregs[REG_EDX],
                            uc->uc_mcontext.gregs[REG_EDI],
                            uc->uc_mcontext.gregs[REG_ESI],
                            uc->uc_mcontext.gregs[REG_EBP],
                            uc->uc_mcontext.gregs[REG_ESP],
                            uc->uc_mcontext.gregs[REG_EIP]);
#elif defined(__x86_64__)
    return tacc_fmt_snprintf(buf, len,
                            "    rax %016lx  rbx %016lx  rcx %016lx  rdx %016lx\n"
                            "    r8  %016lx  r9  %016lx  r10 %016lx  r11 %016lx\n"
                            "    r12 %016lx  r13 %016lx  r14 %016lx  r15 %016lx\n"
                            "    rdi %016lx  rsi %016lx\n"
                            "    rbp %016lx  rsp %016lx  rip %016lx\n\n",
                            uc->uc_mcontext.gregs[REG_RAX],
                            uc->uc_mcontext.gregs[REG_RBX],
                            uc->uc_mcontext.gregs[REG_RCX],
                            uc->uc_mcontext.gregs[REG_RDX],
                            uc->uc_mcontext.gregs[REG_R8],
                            uc->uc_mcontext.gregs[REG_R9],
                            uc->uc_mcontext.gregs[REG_R10],
                            uc->uc_mcontext.gregs[REG_R11],
                            uc->uc_mcontext.gregs[REG_R12],
                            uc->uc_mcontext.gregs[REG_R13],
                            uc->uc_mcontext.gregs[REG_R14],
                            uc->uc_mcontext.gregs[REG_R15],
                            uc->uc_mcontext.gregs[REG_RDI],
                            uc->uc_mcontext.gregs[REG_RSI],
                            uc->uc_mcontext.gregs[REG_RBP],
                            uc->uc_mcontext.gregs[REG_RSP],
                            uc->uc_mcontext.gregs[REG_RIP]);
#endif
}

static size_t tac_fallback_get_backtrace(char *buf, size_t len, siginfo_t *si, ucontext_t *uc)
{
    size_t used = 0;

    used += tacc_fmt_snprintf(buf + used, len - used, "backtrace:\n");
    used += tacc_unwind_get(tac_common_api_level, si, uc, buf + used, len - used);
    if(used >= len - 1)
    {
        buf[len - 3] = '\n';
        buf[len - 2] = '\0';
        used = len - 2;
    }
    used += tacc_fmt_snprintf(buf + used, len - used, "\n");
    return used;
}

size_t tac_fallback_get_emergency(siginfo_t *si,
                                 ucontext_t *uc,
                                 pid_t tid,
                                 uint64_t crash_time,
                                 char *emergency,
                                 size_t emergency_len)
{
    size_t used = tacc_util_get_dump_header(emergency, emergency_len,
                                           TACC_UTIL_CRASH_TYPE_NATIVE,
                                           tac_common_time_zone,
                                           tac_common_start_time,
                                           crash_time,
                                           tac_common_app_id,
                                           tac_common_app_version,
                                           tac_common_api_level,
                                           tac_common_os_version,
                                           tac_common_kernel_version,
                                           tac_common_abi_list,
                                           tac_common_manufacturer,
                                           tac_common_brand,
                                           tac_common_model,
                                           tac_common_build_fingerprint);
    used += tac_fallback_get_process_thread(emergency + used, emergency_len - used, tid);
    used += tac_fallback_get_signal(emergency + used, emergency_len - used, si);
    used += tac_fallback_get_regs(emergency + used, emergency_len - used, uc);
    used += tac_fallback_get_backtrace(emergency + used, emergency_len - used, si, uc);
    return used;
}

int tac_fallback_record(int log_fd, char *emergency) {

    int r;

    if (log_fd < 0) return TAC_ERRNO_INVAL;

    if (0 != (r = tacc_util_write_str(log_fd, emergency))) return r;

    //If we wrote the emergency info successfully, we don't need to return it from callback again.
    emergency[0] = '\0';

    return 0;
}

#pragma clang diagnostic pop

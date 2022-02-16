//
// Created by bugliee on 2022/1/11.
//

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "queue.h"
#include "../tdcrash/tdc_errno.h"
#include "tdcc_util.h"
#include "tdcd_frames.h"
#include "tdcd_md5.h"
#include "tdcd_util.h"
#include "tdcd_elf.h"
#include "tdcd_log.h"

#define TDCD_FRAMES_MAX         256
#define TDCD_FRAMES_STACK_WORDS 16

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct tdcd_frame
{
    tdcd_map_t* map;
    size_t     num;
    uintptr_t  pc;
    uintptr_t  rel_pc;
    uintptr_t  sp;
    char      *func_name;
    size_t     func_offset;
    TAILQ_ENTRY(tdcd_frame,) link;
} tdcd_frame_t;
#pragma clang diagnostic pop
typedef TAILQ_HEAD(tdcd_frame_queue, tdcd_frame,) tdcd_frame_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct tdcd_frames
{
    pid_t              pid;
    tdcd_regs_t        *regs;
    tdcd_maps_t        *maps;
    tdcd_frame_queue_t  frames;
    size_t             frames_num;
};
#pragma clang diagnostic pop

static void tdcd_frames_load(tdcd_frames_t *self)
{
    tdcd_frame_t  *frame;
    tdcd_map_t    *map;
    tdcd_map_t    *map_sp;
    tdcd_elf_t    *elf;
    int           adjust_pc = 0;
    uintptr_t     cur_pc;
    uintptr_t     cur_sp;
    uintptr_t     step_pc;
    uintptr_t     rel_pc;
    uintptr_t     pc_adjustment;
    int           stepped;
    int           in_device_map;
    int           return_address_attempt = 0;
    int           finished;
    int           sigreturn;
    uintptr_t     load_bias;
    tdcd_memory_t *memory;
    tdcd_regs_t    regs_copy = *(self->regs);

    while(self->frames_num < TDCD_FRAMES_MAX)
    {
        memory = NULL;
        map = NULL;
        elf = NULL;
        frame = NULL;
        cur_pc = tdcd_regs_get_pc(&regs_copy);
        cur_sp = tdcd_regs_get_sp(&regs_copy);
        rel_pc = cur_pc;
        step_pc = cur_pc;
        pc_adjustment = 0;
        in_device_map = 0;
        load_bias = 0;
        finished = 0;
        sigreturn = 0;

        //get relative pc
        if(NULL != (map = tdcd_maps_find_map(self->maps, cur_pc)))
        {
            rel_pc = tdcd_map_get_rel_pc(map, step_pc, self->pid, (void *)self->maps);
            step_pc = rel_pc;

            elf = tdcd_map_get_elf(map, self->pid, (void *)self->maps);
            if(NULL != elf)
            {
                load_bias = tdcd_elf_get_load_bias(elf);
                memory = tdcd_elf_get_memory(elf);
            }

            if(adjust_pc)
                pc_adjustment = tdcd_regs_get_adjust_pc(rel_pc, load_bias, memory);
            
            step_pc -= pc_adjustment;
        }
        adjust_pc = 1;

        //create new frame
        if(NULL == (frame = malloc(sizeof(tdcd_frame_t)))) break;
        frame->map = map;
        frame->num = self->frames_num;
        frame->pc = cur_pc - pc_adjustment;
        frame->rel_pc = rel_pc - pc_adjustment;
        frame->sp = cur_sp;
        frame->func_name = NULL;
        frame->func_offset = 0;
        if(NULL != elf)
            tdcd_elf_get_function_info(elf, step_pc, &(frame->func_name), &(frame->func_offset));
        TAILQ_INSERT_TAIL(&(self->frames), frame, link);
        self->frames_num++;

        //step
        if(NULL == map)
        {
            //pc map not found
            stepped = 0;
        }
        else if(map->flags & TDCD_MAP_PORT_DEVICE)
        {
            //pc in device map
            stepped = 0;
            in_device_map = 1;
        }
        else if((NULL != (map_sp = tdcd_maps_find_map(self->maps, cur_sp))) &&
                (map_sp->flags & TDCD_MAP_PORT_DEVICE))
        {
            //sp in device map
            stepped = 0;
            in_device_map = 1;
        }
        else if(NULL == elf)
        {
            //get elf failed
            stepped = 0;
        }
        else
        {
            //do step
#if TDCD_FRAMES_DEBUG
            TDCD_LOG_DEBUG("FRAMES: step, rel_pc=%"PRIxPTR", step_pc=%"PRIxPTR", ELF=%s", rel_pc, step_pc, frame->map->name);
#endif
            if(0 == tdcd_elf_step(elf, rel_pc, step_pc, &regs_copy, &finished, &sigreturn))
                stepped = 1;
            else
                stepped = 0;

            //sigreturn PC should not be adjusted
            if(sigreturn)
            {
                frame->pc += pc_adjustment;
                frame->rel_pc += pc_adjustment;
                step_pc += pc_adjustment;
            }

            //finished gracefully
            if(stepped && finished)
            {
#if TDCD_FRAMES_DEBUG
                TDCD_LOG_DEBUG("FRAMES: all step OK, finished gracefully");
#endif
                break;
            }
        }

        if(0 == stepped)
        {
            //step failed
            if(return_address_attempt)
            {
                if(self->frames_num > 2 || (self->frames_num > 0 && NULL != tdcd_maps_find_map(self->maps, TAILQ_FIRST(&(self->frames))->pc)))
                {
                    TAILQ_REMOVE(&(self->frames), frame, link);
                    self->frames_num--;
                    if(frame->func_name) free(frame->func_name);
                    free(frame);
                }
                break;
            }
            else if(in_device_map)
            {
                break;
            }
            else
            {
                //try this secondary method
                if(0 != tdcd_regs_set_pc_from_lr(&regs_copy, self->pid)) break;
                return_address_attempt = 1;
            }
        }
        else
        {
            //step OK
            return_address_attempt = 0;        
        }

        //If the pc and sp didn't change, then consider everything stopped.
        if(cur_pc == tdcd_regs_get_pc(&regs_copy) && cur_sp == tdcd_regs_get_sp(&regs_copy))
            break;
    }
}

int tdcd_frames_create(tdcd_frames_t **self, tdcd_regs_t *regs, tdcd_maps_t *maps, pid_t pid)
{
    if(NULL == (*self = malloc(sizeof(tdcd_frames_t)))) return TDC_ERRNO_NOMEM;
    (*self)->pid = pid;
    (*self)->regs = regs;
    (*self)->maps = maps;
    TAILQ_INIT(&((*self)->frames));
    (*self)->frames_num = 0;
    
    tdcd_frames_load(*self);
    
    return 0;
}

int tdcd_frames_record_backtrace(tdcd_frames_t *self, int log_fd)
{
    tdcd_frame_t *frame;
    tdcd_elf_t   *elf;
    char        *name;
    char         name_buf[512];
    char        *name_embedded;
    char        *offset;
    char         offset_buf[64];
    char        *func;
    char         func_buf[512];
    int          r;

    if(0 != (r = tdcc_util_write_str(log_fd, "backtrace:\n"))) return r;
    
    TAILQ_FOREACH(frame, &(self->frames), link)
    {
        //name
        name = NULL;
        if(NULL == frame->map)
        {
            name = "<unknown>";
        }
        else if(NULL == frame->map->name || '\0' == frame->map->name[0])
        {
            snprintf(name_buf, sizeof(name_buf), "<anonymous:%"TDCC_UTIL_FMT_ADDR">", frame->map->start);
            name = name_buf;
        }
        else
        {
            if(0 != frame->map->elf_start_offset)
            {
                elf = tdcd_map_get_elf(frame->map, self->pid, (void *)self->maps);
                if(NULL != elf)
                {
                    name_embedded = tdcd_elf_get_so_name(elf);
                    if(NULL != name_embedded && strlen(name_embedded) > 0)
                    {
                        snprintf(name_buf, sizeof(name_buf), "%s!%s", frame->map->name, name_embedded);
                        name = name_buf;
                    }
                }
            }
            if(NULL == name) name = frame->map->name;
        }

        //offset
        if(NULL != frame->map && 0 != frame->map->elf_start_offset)
        {
            snprintf(offset_buf, sizeof(offset_buf), " (offset 0x%"PRIxPTR")", frame->map->elf_start_offset);
            offset = offset_buf;
        }
        else
        {
            offset = "";
        }

        //func
        if(NULL != frame->func_name)
        {
            if(frame->func_offset > 0)
                snprintf(func_buf, sizeof(func_buf), " (%s+%zu)", frame->func_name, frame->func_offset);
            else
                snprintf(func_buf, sizeof(func_buf), " (%s)", frame->func_name);
            func = func_buf;
        }
        else
        {
            func = "";
        }

        if(0 != (r = tdcc_util_write_format(log_fd, "    #%02zu pc %0"TDCC_UTIL_FMT_ADDR"  %s%s%s\n",
                                           frame->num, frame->rel_pc, name, offset, func))) return r;
    }

    if(0 != (r = tdcc_util_write_str(log_fd, "\n"))) return r;

    return 0;
}

static int tdcd_frames_record_buildid_line(tdcd_frames_t *self, const char *name, tdcd_map_t *map, int log_fd, int dump_elf_hash)
{
    char    buf[1024];
    size_t  offset, i;
    char   *error_from = "?";

    //pathname
    offset = (size_t)snprintf(buf, sizeof(buf), "    %s (BuildId: ", name);
    
    //append build-id
    tdcd_elf_t *elf;
    uint8_t build_id[64];
    size_t  build_id_len = 0;
    if(NULL != (elf = tdcd_map_get_elf(map, self->pid, (void *)self->maps)) &&
       0 == tdcd_elf_get_build_id(elf, build_id, sizeof(build_id), &build_id_len))
    {
        for(i = 0; i < build_id_len; i++)
            offset += (size_t)snprintf(buf + offset, sizeof(buf) - offset, "%02hhx", build_id[i]);
    }
    else
    {
        offset += (size_t)snprintf(buf + offset, sizeof(buf) - offset, "%s", "unknown");
    }

    //open file
    int fd;
    errno = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
    if(0 > (fd = TDCC_UTIL_TEMP_FAILURE_RETRY(open(name, O_RDONLY | O_CLOEXEC))))
    {
        error_from = "OPEN";
        goto err;
    }
#pragma clang diagnostic pop

    //get file status
    struct stat st;
    errno = 0; 
    if(0 != fstat(fd, &st))
    {
        error_from = "FSTAT";
        goto err;
    }
    
    //append file-size
    offset += (size_t)snprintf(buf + offset, sizeof(buf) - offset, ". FileSize: %ld",
                               (long)st.st_size);

    //append last-modified
    struct tm tm;
    if(NULL != localtime_r(&(st.st_mtim.tv_sec), &tm))
    {
        offset += (size_t)snprintf(buf + offset, sizeof(buf) - offset, ". LastModified: %04d-%02d-%02dT%02d:%02d:%02d.%03ld%c%02ld%02ld",
                                   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                   tm.tm_hour, tm.tm_min, tm.tm_sec, st.st_mtim.tv_nsec / 1000000,
                                   tm.tm_gmtoff < 0 ? '-' : '+', labs(tm.tm_gmtoff / 3600), labs(tm.tm_gmtoff % 3600));
    }
    else
    {
        offset += (size_t)snprintf(buf + offset, sizeof(buf) - offset, ". LastModified: %s", "unknown");
    }

    //append md5
    if(dump_elf_hash)
    {
        size_t name_len = strlen(name);
        if(st.st_size > 0
           && ((name_len > 3 && 0 == memcmp(name + name_len - 3, ".so", 3))
               || (name_len > 12 && 0 == memcmp(name, "/system/bin/", 12))))
        {
            errno = 0;
            uint8_t *data = (uint8_t *)mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if(data == MAP_FAILED)
            {
                error_from = "MMAP";
                goto err;
            }
            
            uint8_t md5[16];
            tdcd_MD5_CTX ctx;
            tdcd_MD5_Init(&ctx);
            tdcd_MD5_Update(&ctx, data, (unsigned long)st.st_size);
            tdcd_MD5_Final(md5, &ctx);
            
            munmap(data, (size_t)st.st_size);
            
            offset += (size_t)snprintf(buf + offset, sizeof(buf) - offset, "%s", ". MD5: ");
            for(i = 0; i < sizeof(md5); i++)
                offset += (size_t)snprintf(buf + offset, sizeof(buf) - offset, "%02hhx", md5[i]);
        }
    }

    snprintf(buf + offset, sizeof(buf) - offset, "%s", ")\n");

    //done
    goto end;

 err:
    snprintf(buf + offset, sizeof(buf) - offset, ". %s error: errno = %d, errmsg = %s)\n", error_from, errno, strerror(errno));

 end:
    if(fd >= 0) close(fd);
    return tdcc_util_write_str(log_fd, buf);
}

int tdcd_frames_record_buildid(tdcd_frames_t *self, int log_fd, int dump_elf_hash, uintptr_t fault_addr)
{
    tdcd_frame_t *frame, *prev_frame;
    char        *name, *prev_name, *fault_addr_name = NULL;
    tdcd_map_t   *map;
    int          repeated;
    int          r;

    if(0 != (r = tdcc_util_write_str(log_fd, "build id:\n"))) return r;

    if(fault_addr > 0)
    {
        if(NULL != (map = tdcd_maps_find_map(self->maps, fault_addr)))
        {
            if(NULL != map->name && '\0' != map->name[0])
            {
                if(0 != (r = tdcd_frames_record_buildid_line(self, map->name, map, log_fd, dump_elf_hash))) return r;
                fault_addr_name = map->name;
            }
        }
    }

    TAILQ_FOREACH(frame, &(self->frames), link)
    {
        if(NULL == frame->map || NULL == frame->map->name || '/' != frame->map->name[0]) continue;
        
        //get name
        name = frame->map->name;

        //check repeated
        if(NULL != fault_addr_name && 0 == strcmp(name, fault_addr_name)) continue;
        repeated = 0;
        prev_frame = frame;
        while(NULL != (prev_frame = TAILQ_PREV(prev_frame, tdcd_frame_queue, link)))
        {
            if(NULL == prev_frame->map || NULL == prev_frame->map->name || '\0' == prev_frame->map->name[0]) continue;
            prev_name = prev_frame->map->name;
            if(0 == strcmp(name, prev_name))
            {
                repeated = 1;
                break;
            }
        }
        if(repeated) continue;

        if(0 != (r = tdcd_frames_record_buildid_line(self, name, frame->map, log_fd, dump_elf_hash))) return r;
    }

    if(0 != (r = tdcc_util_write_str(log_fd, "\n"))) return r;

    return 0;
}

static int tdcd_frames_record_stack_segment(tdcd_frames_t *self, int log_fd,
                                           uintptr_t *sp, size_t words, int label)
{
    uintptr_t  stack_data[TDCD_FRAMES_STACK_WORDS];
    size_t     bytes_read;
    size_t     i;
    char       line[512];
    size_t     line_len = 0;
    tdcd_map_t *map;
    tdcd_elf_t *elf;
    uintptr_t  rel_pc;
    char      *name_embedded;
    char      *func_name;
    size_t     func_offset;
    int        r;

    //read remote data
    bytes_read = tdcd_util_ptrace_read(self->pid, *sp, stack_data, sizeof(uintptr_t) * words);
    words = bytes_read / sizeof(uintptr_t);

    //print
    for(i = 0; i < words; i++)
    {
        //num
        if(i == 0 && label >= 0)
            line_len = (size_t)snprintf(line, sizeof(line), "    #%02d  ", label);
        else
            line_len = (size_t)snprintf(line, sizeof(line), "         ");

        //addr, data
        line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len,
                                     "%0"TDCC_UTIL_FMT_ADDR"  %0"TDCC_UTIL_FMT_ADDR, *sp, stack_data[i]);

        //file, func-name, func-offset
        map = NULL;
        func_name = NULL;
        func_offset = 0;
        if(NULL != (map = tdcd_maps_find_map(self->maps, stack_data[i])) &&
           NULL != map->name && '\0' != map->name[0])
        {
            line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len,
                                         "  %s", map->name);

            if(NULL != (elf = tdcd_map_get_elf(map, self->pid, (void *)self->maps)))
            {
                if(0 != map->elf_start_offset)
                {
                    name_embedded = tdcd_elf_get_so_name(elf);
                    if(NULL != name_embedded && strlen(name_embedded) > 0)
                    {
                        line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len, "!%s", name_embedded);
                    }
                }

                rel_pc = tdcd_map_get_rel_pc(map, stack_data[i], self->pid, (void *)self->maps);
                
                func_name = NULL;
                func_offset = 0;
                tdcd_elf_get_function_info(elf, rel_pc, &func_name, &func_offset);

                if(NULL != func_name)
                {
                    if(func_offset > 0)
                        line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len,
                                                     " (%s+%zu)", func_name, func_offset);
                    else
                        line_len += (size_t)snprintf(line + line_len, sizeof(line) - line_len,
                                                     " (%s)", func_name);
                }
            }
        }
        if(NULL != func_name) free(func_name);

        snprintf(line + line_len, sizeof(line) - line_len, "\n");
        if(0 != (r = tdcc_util_write_str(log_fd, line))) return r;
        
        *sp += sizeof(uintptr_t);
    }

    return 0;
}

int tdcd_frames_record_stack(tdcd_frames_t *self, int log_fd)
{
    int          segment_recorded = 0;
    tdcd_frame_t *frame, *next_frame;
    uintptr_t    sp = 0;
    size_t       stack_size;
    size_t       words;
    int          r;
    
    if(0 != (r = tdcc_util_write_str(log_fd, "stack:\n"))) return r;

    TAILQ_FOREACH(frame, &(self->frames), link)
    {
        if(0 == frame->sp)
        {
            if(segment_recorded)
                break;
            else
                continue;
        }

        //dump a few words before the first frame
        if(!segment_recorded)
        {
            segment_recorded = 1;
            sp = frame->sp - TDCD_FRAMES_STACK_WORDS * sizeof(uintptr_t);
            tdcd_frames_record_stack_segment(self, log_fd, &sp, TDCD_FRAMES_STACK_WORDS, -1);
        }

        if(sp != frame->sp)
        {
            if(0 != (r = tdcc_util_write_str(log_fd, "         ........  ........\n"))) return r;
            sp = frame->sp;
        }

        next_frame = TAILQ_NEXT(frame, link);
        if(NULL == next_frame || 0 == next_frame->sp || next_frame->sp < frame->sp)
        {
            //the last
            tdcd_frames_record_stack_segment(self, log_fd, &sp, TDCD_FRAMES_STACK_WORDS, (int)frame->num);
        }
        else
        {
            //others
            stack_size = next_frame->sp - frame->sp;
            words = stack_size / sizeof(uintptr_t);
            if(0 == words)
                words = 1;
            else if(words > TDCD_FRAMES_STACK_WORDS)
                words = TDCD_FRAMES_STACK_WORDS;
            tdcd_frames_record_stack_segment(self, log_fd, &sp, words, (int)frame->num);
        }
        
    }

    if(0 != (r = tdcc_util_write_str(log_fd, "\n"))) return r;

    return 0;
}

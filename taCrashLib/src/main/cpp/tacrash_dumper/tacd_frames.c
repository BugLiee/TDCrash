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
#include "../tacrash/tac_errno.h"
#include "tacc_util.h"
#include "tacd_frames.h"
#include "tacd_md5.h"
#include "tacd_util.h"
#include "tacd_elf.h"
#include "tacd_log.h"

#define TACD_FRAMES_MAX         256

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct tacd_frame
{
    tacd_map_t* map;
    size_t     num;
    uintptr_t  pc;
    uintptr_t  rel_pc;
    uintptr_t  sp;
    char      *func_name;
    size_t     func_offset;
    TAILQ_ENTRY(tacd_frame,) link;
} tacd_frame_t;
#pragma clang diagnostic pop
typedef TAILQ_HEAD(tacd_frame_queue, tacd_frame,) tacd_frame_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct tacd_frames
{
    pid_t              pid;
    tacd_regs_t        *regs;
    tacd_maps_t        *maps;
    tacd_frame_queue_t  frames;
    size_t             frames_num;
};
#pragma clang diagnostic pop

static void tacd_frames_load(tacd_frames_t *self)
{
    tacd_frame_t  *frame;
    tacd_map_t    *map;
    tacd_map_t    *map_sp;
    tacd_elf_t    *elf;
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
    tacd_memory_t *memory;
    tacd_regs_t    regs_copy = *(self->regs);

    while(self->frames_num < TACD_FRAMES_MAX)
    {
        memory = NULL;
        map = NULL;
        elf = NULL;
        frame = NULL;
        cur_pc = tacd_regs_get_pc(&regs_copy);
        cur_sp = tacd_regs_get_sp(&regs_copy);
        rel_pc = cur_pc;
        step_pc = cur_pc;
        pc_adjustment = 0;
        in_device_map = 0;
        load_bias = 0;
        finished = 0;
        sigreturn = 0;

        //get relative pc
        if(NULL != (map = tacd_maps_find_map(self->maps, cur_pc)))
        {
            rel_pc = tacd_map_get_rel_pc(map, step_pc, self->pid, (void *)self->maps);
            step_pc = rel_pc;

            elf = tacd_map_get_elf(map, self->pid, (void *)self->maps);
            if(NULL != elf)
            {
                load_bias = tacd_elf_get_load_bias(elf);
                memory = tacd_elf_get_memory(elf);
            }

            if(adjust_pc)
                pc_adjustment = tacd_regs_get_adjust_pc(rel_pc, load_bias, memory);
            
            step_pc -= pc_adjustment;
        }
        adjust_pc = 1;

        //create new frame
        if(NULL == (frame = malloc(sizeof(tacd_frame_t)))) break;
        frame->map = map;
        frame->num = self->frames_num;
        frame->pc = cur_pc - pc_adjustment;
        frame->rel_pc = rel_pc - pc_adjustment;
        frame->sp = cur_sp;
        frame->func_name = NULL;
        frame->func_offset = 0;
        if(NULL != elf)
            tacd_elf_get_function_info(elf, step_pc, &(frame->func_name), &(frame->func_offset));
        TAILQ_INSERT_TAIL(&(self->frames), frame, link);
        self->frames_num++;

        //step
        if(NULL == map)
        {
            //pc map not found
            stepped = 0;
        }
        else if(map->flags & TACD_MAP_PORT_DEVICE)
        {
            //pc in device map
            stepped = 0;
            in_device_map = 1;
        }
        else if((NULL != (map_sp = tacd_maps_find_map(self->maps, cur_sp))) &&
                (map_sp->flags & TACD_MAP_PORT_DEVICE))
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
#if TACD_FRAMES_DEBUG
            TACD_LOG_DEBUG("FRAMES: step, rel_pc=%"PRIxPTR", step_pc=%"PRIxPTR", ELF=%s", rel_pc, step_pc, frame->map->name);
#endif
            if(0 == tacd_elf_step(elf, rel_pc, step_pc, &regs_copy, &finished, &sigreturn))
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
#if TACD_FRAMES_DEBUG
                TACD_LOG_DEBUG("FRAMES: all step OK, finished gracefully");
#endif
                break;
            }
        }

        if(0 == stepped)
        {
            //step failed
            if(return_address_attempt)
            {
                if(self->frames_num > 2 || (self->frames_num > 0 && NULL != tacd_maps_find_map(self->maps, TAILQ_FIRST(&(self->frames))->pc)))
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
                if(0 != tacd_regs_set_pc_from_lr(&regs_copy, self->pid)) break;
                return_address_attempt = 1;
            }
        }
        else
        {
            //step OK
            return_address_attempt = 0;        
        }

        //If the pc and sp didn't change, then consider everything stopped.
        if(cur_pc == tacd_regs_get_pc(&regs_copy) && cur_sp == tacd_regs_get_sp(&regs_copy))
            break;
    }
}

int tacd_frames_create(tacd_frames_t **self, tacd_regs_t *regs, tacd_maps_t *maps, pid_t pid)
{
    if(NULL == (*self = malloc(sizeof(tacd_frames_t)))) return TAC_ERRNO_NOMEM;
    (*self)->pid = pid;
    (*self)->regs = regs;
    (*self)->maps = maps;
    TAILQ_INIT(&((*self)->frames));
    (*self)->frames_num = 0;
    
    tacd_frames_load(*self);
    
    return 0;
}

int tacd_frames_record_backtrace(tacd_frames_t *self, int log_fd)
{
    tacd_frame_t *frame;
    tacd_elf_t   *elf;
    char        *name;
    char         name_buf[512];
    char        *name_embedded;
    char        *offset;
    char         offset_buf[64];
    char        *func;
    char         func_buf[512];
    int          r;

    if(0 != (r = tacc_util_write_str(log_fd, "backtrace:\n"))) return r;
    
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
            snprintf(name_buf, sizeof(name_buf), "<anonymous:%"TACC_UTIL_FMT_ADDR">", frame->map->start);
            name = name_buf;
        }
        else
        {
            if(0 != frame->map->elf_start_offset)
            {
                elf = tacd_map_get_elf(frame->map, self->pid, (void *)self->maps);
                if(NULL != elf)
                {
                    name_embedded = tacd_elf_get_so_name(elf);
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

        if(0 != (r = tacc_util_write_format(log_fd, "    #%02zu pc %0"TACC_UTIL_FMT_ADDR"  %s%s%s\n",
                                           frame->num, frame->rel_pc, name, offset, func))) return r;
    }

    if(0 != (r = tacc_util_write_str(log_fd, "\n"))) return r;

    return 0;
}





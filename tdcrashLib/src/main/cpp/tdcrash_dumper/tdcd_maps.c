//
// Created by bugliee on 2022/1/11.
//

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "queue.h"
#include "../tdcrash/tdc_errno.h"
#include "tdcc_util.h"
#include "tdcd_maps.h"
#include "tdcd_map.h"
#include "tdcd_util.h"
#include "tdcd_log.h"

#define TDCD_MAPS_ABORT_MSG_NAME    "[anon:abort message]"
#define TDCD_MAPS_ABORT_MSG_FLAGS   (PROT_READ | PROT_WRITE)
#define TDCD_MAPS_ABORT_MSG_MAGIC_1 0xb18e40886ac388f0ULL
#define TDCD_MAPS_ABORT_MSG_MAGIC_2 0xc6dfba755a1de0b5ULL

typedef struct tdcd_maps_item
{
    tdcd_map_t map;
    TAILQ_ENTRY(tdcd_maps_item,) link;
} tdcd_maps_item_t;
typedef TAILQ_HEAD(tdcd_maps_item_queue, tdcd_maps_item,) tdcd_maps_item_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct tdcd_maps
{
    tdcd_maps_item_queue_t maps;
    pid_t                 pid;
};
#pragma clang diagnostic pop

static int tdcd_maps_parse_line(char *line, tdcd_maps_item_t **mi)
{
    uintptr_t  start;
    uintptr_t  end;
    char       flags[5];
    size_t     offset;
    int        pos;
    char      *name;

    *mi = NULL;
    
    //scan
    if(sscanf(line, "%"SCNxPTR"-%"SCNxPTR" %4s %"SCNxPTR" %*x:%*x %*d%n", &start, &end, flags, &offset, &pos) != 4) return 0;
    name = tdcc_util_trim(line + pos);
    
    //create map
    if(NULL == (*mi = malloc(sizeof(tdcd_maps_item_t)))) return TDC_ERRNO_NOMEM;
    return tdcd_map_init(&((*mi)->map), start, end, offset, flags, name);
}

int tdcd_maps_create(tdcd_maps_t **self, pid_t pid)
{
    char             buf[512];
    FILE            *fp;
    tdcd_maps_item_t *mi;
    int              r;

    if(NULL == (*self = malloc(sizeof(tdcd_maps_t)))) return TDC_ERRNO_NOMEM;
    TAILQ_INIT(&((*self)->maps));
    (*self)->pid = pid;

    snprintf(buf, sizeof(buf), "/proc/%d/maps", pid);
    if(NULL == (fp = fopen(buf, "r"))) return TDC_ERRNO_SYS;

    while(fgets(buf, sizeof(buf), fp))
    {
        if(0 != (r = tdcd_maps_parse_line(buf, &mi)))
        {
            fclose(fp);
            return r;
        }
        
        if(NULL != mi)
            TAILQ_INSERT_TAIL(&((*self)->maps), mi, link);
    }
    
    fclose(fp);
    return 0;
}

void tdcd_maps_destroy(tdcd_maps_t **self)
{
    tdcd_maps_item_t *mi, *mi_tmp;
    TAILQ_FOREACH_SAFE(mi, &((*self)->maps), link, mi_tmp)
    {
        TAILQ_REMOVE(&((*self)->maps), mi, link);
        tdcd_map_uninit(&(mi->map));
        free(mi);
    }

    *self = NULL;
}

tdcd_map_t *tdcd_maps_find_map(tdcd_maps_t *self, uintptr_t pc)
{
    tdcd_maps_item_t *mi;

    TAILQ_FOREACH(mi, &(self->maps), link)
        if(pc >= mi->map.start && pc < mi->map.end)
            return &(mi->map);

    return NULL;
}

tdcd_map_t *tdcd_maps_get_prev_map(tdcd_maps_t *self, tdcd_map_t *cur_map)
{
    (void)self;

    tdcd_maps_item_t *cur_mi = (tdcd_maps_item_t *)cur_map;
    tdcd_maps_item_t *prev_mi = TAILQ_PREV(cur_mi, tdcd_maps_item_queue, link);

    return (NULL == prev_mi ? NULL : &(prev_mi->map));
}

uintptr_t tdcd_maps_find_abort_msg(tdcd_maps_t *self)
{
    tdcd_maps_item_t *mi;
    uintptr_t        p;
    uint64_t         magic;

    TAILQ_FOREACH(mi, &(self->maps), link)
    {
        if(NULL != mi->map.name && 0 == strcmp(mi->map.name, TDCD_MAPS_ABORT_MSG_NAME) &&
           TDCD_MAPS_ABORT_MSG_FLAGS == mi->map.flags)
        {
            p = mi->map.start;
            if(0 != tdcd_util_ptrace_read_fully(self->pid, p, &magic, sizeof(uint64_t))) continue;
            if(TDCD_MAPS_ABORT_MSG_MAGIC_1 != magic) continue;

            p += sizeof(uint64_t);
            if(0 != tdcd_util_ptrace_read_fully(self->pid, p, &magic, sizeof(uint64_t))) continue;
            if(TDCD_MAPS_ABORT_MSG_MAGIC_2 != magic) continue;

            return mi->map.start;
        }
    }

    return 0;
}

uintptr_t tdcd_maps_find_pc(tdcd_maps_t *self, const char *pathname, const char *symbol)
{
    tdcd_maps_item_t *mi;
    tdcd_elf_t       *elf;
    uintptr_t        addr = 0;

    TAILQ_FOREACH(mi, &(self->maps), link)
    {
        if(NULL != mi->map.name && 0 == strcmp(mi->map.name, pathname))
        {
            //get ELF
            if(NULL == (elf = tdcd_map_get_elf(&(mi->map), self->pid, (void *)self))) return 0;

            //get rel addr (offset)
            if(0 != tdcd_elf_get_symbol_addr(elf, symbol, &addr)) return 0;

            return tdcd_map_get_abs_pc(&(mi->map), addr, self->pid, (void *)self);
        }
    }

    return 0; //not found
}

int tdcd_maps_record(tdcd_maps_t *self, int log_fd)
{
    int              r;
    tdcd_maps_item_t *mi;
    uintptr_t        size;
    uintptr_t        total_size = 0;
    size_t           max_size = 0;
    size_t           max_offset = 0;
    size_t           width_size = 0;
    size_t           width_offset = 0;
    uintptr_t        load_bias = 0;
    char             load_bias_buf[64] = "\0";
    char            *name;
    char            *prev_name = NULL;

    //get width of size and offset columns
    TAILQ_FOREACH(mi, &(self->maps), link)
    {
        size = mi->map.end - mi->map.start;
        if(size > max_size) max_size = size;
        if(mi->map.offset > max_offset) max_offset = mi->map.offset;
    }
    while(0 != max_size)
    {
        max_size /= 0x10;
        width_size++;
    }
    if(0 == width_size) width_size = 1;
    while(0 != max_offset)
    {
        max_offset /= 0x10;
        width_offset++;
    }
    if(0 == width_offset) width_offset = 1;

    //dump
    if(0 != (r = tdcc_util_write_str(log_fd, "memory map:\n"))) return r;
    TAILQ_FOREACH(mi, &(self->maps), link)
    {
        //get load_bias
        if(NULL != mi->map.elf && 0 != (load_bias = tdcd_elf_get_load_bias(mi->map.elf)))
            snprintf(load_bias_buf, sizeof(load_bias_buf), " (load bias 0x%"PRIxPTR")", load_bias);
        else
            load_bias_buf[0] = '\0';

        //fix name and load_bias
        if(NULL != mi->map.name)
        {
            if(NULL == prev_name)
                name = mi->map.name;
            else if(0 == strcmp(prev_name, mi->map.name) && '\0' == load_bias_buf[0])
                name = ">"; //same as prev line
            else
                name = mi->map.name;
        }
        else
        {
            name = "";
        }

        //save prev name
        prev_name = mi->map.name;

        //update total size
        size = mi->map.end - mi->map.start;
        total_size += size;

        if(0 != (r = tdcc_util_write_format(log_fd,
                                           "    %0"TDCC_UTIL_FMT_ADDR"-%0"TDCC_UTIL_FMT_ADDR" %c%c%c %*"PRIxPTR" %*"PRIxPTR" %s%s\n",
                                           mi->map.start, mi->map.end,
                                           mi->map.flags & PROT_READ ? 'r' : '-',
                                           mi->map.flags & PROT_WRITE ? 'w' : '-',
                                           mi->map.flags & PROT_EXEC ? 'x' : '-',
                                           width_offset, mi->map.offset,
                                           width_size, size,
                                           name, load_bias_buf))) return r;
    }
    if(0 != (r = tdcc_util_write_format(log_fd, "    TOTAL SIZE: 0x%"PRIxPTR"K (%"PRIuPTR"K)\n\n",
                                       total_size / 1024, total_size / 1024))) return r;

    return 0;
}

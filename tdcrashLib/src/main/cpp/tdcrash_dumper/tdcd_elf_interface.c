//
// Created by bugliee on 2022/1/11.
//

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <link.h>
#include <elf.h>
#include <sys/types.h>
#include "../tdcrash/tdc_errno.h"
#include "tdcd_elf_interface.h"
#include "tdcd_dwarf.h"
#include "tdcd_arm_exidx.h"
#include "tdcd_memory.h"
#include "tdcd_log.h"
#include "tdcd_util.h"
#include "queue.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct tdcd_elf_load
{
    uintptr_t vaddr;
    size_t    offset;
    size_t    size;
    TAILQ_ENTRY(tdcd_elf_load,) link;
} tdcd_elf_load_t;
#pragma clang diagnostic pop
typedef TAILQ_HEAD(tdcd_elf_load_queue, tdcd_elf_load,) tdcd_elf_load_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct tdcd_elf_symbols
{
    size_t sym_offset;
    size_t sym_end;
    size_t sym_entry_size;
    size_t str_offset;
    size_t str_end;
    TAILQ_ENTRY(tdcd_elf_symbols,) link;
} tdcd_elf_symbols_t;
#pragma clang diagnostic pop
typedef TAILQ_HEAD(tdcd_elf_symbols_queue, tdcd_elf_symbols,) tdcd_elf_symbols_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct tdcd_elf_strtab
{
    size_t addr;
    size_t offset;
    TAILQ_ENTRY(tdcd_elf_strtab,) link;
} tdcd_elf_strtab_t;
#pragma clang diagnostic pop
typedef TAILQ_HEAD(tdcd_elf_strtab_queue, tdcd_elf_strtab,) tdcd_elf_strtab_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct tdcd_elf_interface
{
    pid_t                    pid;
    tdcd_memory_t            *memory;
    char                    *so_name;
    uintptr_t                load_bias;
    int                      is_gnu;

    //symbols (.dynsym with .dynstr, .symtab with .strtab)
    tdcd_elf_symbols_queue_t  symbolsq;

    //string tables
    tdcd_elf_strtab_queue_t   strtabq;

    //.note.gnu.build-id
    size_t                   build_id_offset;
    size_t                   build_id_size;

    //.eh_frame with option .eh_frame_hdr
    size_t                   eh_frame_offset;
    size_t                   eh_frame_size;
    uintptr_t                eh_frame_load_bias;
    size_t                   eh_frame_hdr_offset;
    size_t                   eh_frame_hdr_size;
    uintptr_t                eh_frame_hdr_load_bias;
    tdcd_dwarf_t             *dwarf_eh_frame;
    tdcd_dwarf_type_t         dwarf_eh_frame_type;

    //.debug_frame
    size_t                   debug_frame_offset;
    size_t                   debug_frame_size;
    tdcd_dwarf_t             *dwarf_debug_frame;

    //.ARM.exidx
    size_t                   arm_exidx_offset;
    size_t                   arm_exidx_size;

    //.gnu_debugdata
    size_t                   gnu_debugdata_offset;
    size_t                   gnu_debugdata_size;

    //.dynamic
    size_t                   dynamic_offset;
    size_t                   dynamic_size;
};
#pragma clang diagnostic pop

static int tdcd_elf_interface_check_valid(ElfW(Ehdr) *ehdr)
{
    //check magic
    if(0 != memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) return TDC_ERRNO_FORMAT;

    //check class (64/32)
#if defined(__LP64__)
    if(ELFCLASS64 != ehdr->e_ident[EI_CLASS]) return TDC_ERRNO_FORMAT;
#else
    if(ELFCLASS32 != ehdr->e_ident[EI_CLASS]) return TDC_ERRNO_FORMAT;
#endif

    //check endian (little/big)
    if(ELFDATA2LSB != ehdr->e_ident[EI_DATA]) return TDC_ERRNO_FORMAT;

    //check version
    if(EV_CURRENT != ehdr->e_ident[EI_VERSION]) return TDC_ERRNO_FORMAT;

    //check type
    if(ET_EXEC != ehdr->e_type && ET_DYN != ehdr->e_type) return TDC_ERRNO_FORMAT;

    //check machine
#if defined(__arm__)
    if(EM_ARM != ehdr->e_machine) return TDC_ERRNO_FORMAT;
#elif defined(__aarch64__)
    if(EM_AARCH64 != ehdr->e_machine) return TDC_ERRNO_FORMAT;
#elif defined(__i386__)
    if(EM_386 != ehdr->e_machine) return TDC_ERRNO_FORMAT;
#elif defined(__x86_64__)
    if(EM_X86_64 != ehdr->e_machine) return TDC_ERRNO_FORMAT;
#else
    return TDC_ERRNO_FORMAT;
#endif

    //check version
    if(EV_CURRENT != ehdr->e_version) return TDC_ERRNO_FORMAT;

    return 0;
}

static int tdcd_elf_interface_read_program_headers(tdcd_elf_interface_t *self, ElfW(Ehdr) *ehdr, uintptr_t *load_bias)
{
    size_t     i;
    ElfW(Phdr) phdr;
    int        try_save_load_bias = 0;
    int        r;
    
    for(i = 0; i < ehdr->e_phnum * ehdr->e_phentsize; i += ehdr->e_phentsize)
    {
        if(0 != tdcd_memory_read_fully(self->memory, ehdr->e_phoff + i, &phdr, sizeof(phdr)))
        {
            r = TDC_ERRNO_MEM;
            goto err;
        }

        switch(phdr.p_type)
        {
        case PT_LOAD:
            {
                if(0 == (phdr.p_flags & PF_X)) continue;

                if(!try_save_load_bias && phdr.p_vaddr > phdr.p_offset)
                {
                    self->load_bias = phdr.p_vaddr - phdr.p_offset;
                    if(NULL != load_bias) *load_bias = self->load_bias;
                }
                try_save_load_bias = 1;
                break;
            }
        case PT_GNU_EH_FRAME:
            self->eh_frame_hdr_offset = phdr.p_offset;
            self->eh_frame_hdr_size = phdr.p_memsz;
            self->eh_frame_hdr_load_bias = phdr.p_vaddr - phdr.p_offset;
            break;
        case PT_ARM_EXIDX:
            self->arm_exidx_offset = phdr.p_offset;
            self->arm_exidx_size = phdr.p_memsz;
            break;
        case PT_DYNAMIC:
            self->dynamic_offset = phdr.p_offset;
            self->dynamic_size = phdr.p_memsz;
            break;
        default:
            break;
        }
    }
    return 0;

 err:
    return r;
}

static int tdcd_elf_interface_read_section_headers(tdcd_elf_interface_t *self, ElfW(Ehdr) *ehdr)
{
    size_t             i;
    ElfW(Shdr)         shdr;
    ElfW(Shdr)         str_shdr;
    size_t             sec_offset = 0;
    size_t             sec_size   = 0;
    tdcd_elf_symbols_t *symbols, *symbols_tmp;
    tdcd_elf_strtab_t  *strtab, *strtab_tmp;
    char               name[128];
    int                r;

    //get section header entry that contains the section names
    if(ehdr->e_shstrndx < ehdr->e_shnum)
    {
        if(0 != tdcd_memory_read_fully(self->memory, ehdr->e_shoff + ehdr->e_shstrndx * ehdr->e_shentsize,
                                      &shdr, sizeof(shdr))) return TDC_ERRNO_MEM;
        sec_offset = shdr.sh_offset;
        sec_size   = shdr.sh_size;
    }

    for(i = ehdr->e_shentsize; i < ehdr->e_shnum * ehdr->e_shentsize; i += ehdr->e_shentsize)
    {
        if(0 != tdcd_memory_read_fully(self->memory, ehdr->e_shoff + i, &shdr, sizeof(shdr)))
        {
            r = TDC_ERRNO_MEM;
            goto err;
        }

        switch(shdr.sh_type)
        {
        case SHT_NOTE:
            {
                if(shdr.sh_name >= sec_size) continue;
                if(0 != tdcd_memory_read_string(self->memory, sec_offset + shdr.sh_name, name, sizeof(name), UINTPTR_MAX))
                    continue;
                
                if(0 == strcmp(name, ".note.gnu.build-id"))
                {
                    self->build_id_offset = shdr.sh_offset;
                    self->build_id_size = shdr.sh_size;
                }
            }
        case SHT_SYMTAB:
        case SHT_DYNSYM:
            {
                //get the associated strtab section
                if(shdr.sh_link >= ehdr->e_shnum) continue;
                if(0 != tdcd_memory_read_fully(self->memory, ehdr->e_shoff + shdr.sh_link * ehdr->e_shentsize, &str_shdr, sizeof(str_shdr)))
                {
                    r = TDC_ERRNO_MEM;
                    goto err;
                }
                if(SHT_STRTAB != str_shdr.sh_type) continue;

                //save symbols and the associated strtab
                if(NULL == (symbols = malloc(sizeof(tdcd_elf_symbols_t))))
                {
                    r = TDC_ERRNO_NOMEM;
                    goto err;
                }
                symbols->sym_offset = shdr.sh_offset;
                symbols->sym_end = shdr.sh_offset + shdr.sh_size;
                symbols->sym_entry_size = shdr.sh_entsize;
                symbols->str_offset = str_shdr.sh_offset;
                symbols->str_end = str_shdr.sh_offset + str_shdr.sh_size;
                TAILQ_INSERT_TAIL(&(self->symbolsq), symbols, link);
                break;
            }
        case SHT_STRTAB:
            {
                if(NULL == (strtab = malloc(sizeof(tdcd_elf_strtab_t))))
                {
                    r = TDC_ERRNO_NOMEM;
                    goto err;
                }
                strtab->addr = shdr.sh_addr;
                strtab->offset = shdr.sh_offset;
                TAILQ_INSERT_TAIL(&(self->strtabq), strtab, link);
                break;
            }
        case SHT_PROGBITS:
            {
                if(shdr.sh_name >= sec_size) continue;
                if(0 != tdcd_memory_read_string(self->memory, sec_offset + shdr.sh_name, name, sizeof(name), UINTPTR_MAX))
                {
                    r = TDC_ERRNO_MEM;
                    goto err;
                }
                
                if(0 == strcmp(name, ".debug_frame"))
                {
                    self->debug_frame_offset = shdr.sh_offset;
                    self->debug_frame_size = shdr.sh_size;
                }
                else if(0 == strcmp(name, ".eh_frame"))
                {
                    self->eh_frame_offset = shdr.sh_offset;
                    self->eh_frame_size = shdr.sh_size;
                    self->eh_frame_load_bias = shdr.sh_addr - shdr.sh_offset;
                }
                else if(0 == strcmp(name, ".eh_frame_hdr") && 0 == self->eh_frame_hdr_offset)
                {
                    self->eh_frame_hdr_offset = shdr.sh_offset;
                    self->eh_frame_hdr_size = shdr.sh_size;
                    self->eh_frame_hdr_load_bias = shdr.sh_addr - shdr.sh_offset;
                }
                else if(0 == strcmp(name, ".gnu_debugdata"))
                {
                    self->gnu_debugdata_offset = shdr.sh_offset;
                    self->gnu_debugdata_size = shdr.sh_size;                    
                }
                break;
            }
        default:
            break;
        }
    }

    return 0;
    
 err:
    TAILQ_FOREACH_SAFE(symbols, &(self->symbolsq), link, symbols_tmp)
    {
        TAILQ_REMOVE(&(self->symbolsq), symbols, link);
        free(symbols);
    }
    TAILQ_FOREACH_SAFE(strtab, &(self->strtabq), link, strtab_tmp)
    {
        TAILQ_REMOVE(&(self->strtabq), strtab, link);
        free(strtab);
    }
    return r;
}

int tdcd_elf_interface_create(tdcd_elf_interface_t **self, pid_t pid, tdcd_memory_t *memory, uintptr_t *load_bias)
{
    int r;
    ElfW(Ehdr) ehdr;

    //get ELF header
    if(0 != tdcd_memory_read_fully(memory, 0, &ehdr, sizeof(ehdr))) return TDC_ERRNO_MEM;
    
    //check valid
    if(0 != (r = tdcd_elf_interface_check_valid(&ehdr))) return r;

    //init
    if(NULL == (*self = calloc(1, sizeof(tdcd_elf_interface_t)))) return TDC_ERRNO_NOMEM;
    (*self)->pid = pid;
    (*self)->memory = memory;
    TAILQ_INIT(&((*self)->symbolsq));
    TAILQ_INIT(&((*self)->strtabq));

    //read program headers, save and return load_bias
    if(0 != (r = tdcd_elf_interface_read_program_headers(*self, &ehdr, load_bias)))
    {
        free(*self);
        *self = NULL;
        return r;
    }

    //read section headers
    tdcd_elf_interface_read_section_headers(*self, &ehdr);

    //dwarf from .eh_frame with .eh_frame_hdr
    if(0 != (*self)->eh_frame_hdr_offset && 0 != (*self)->eh_frame_hdr_size)
    {
        tdcd_dwarf_create(&((*self)->dwarf_eh_frame), memory, pid,
                         (*self)->eh_frame_load_bias, (*self)->eh_frame_hdr_load_bias,
                         (*self)->eh_frame_hdr_offset, (*self)->eh_frame_hdr_size,
                         TDCD_DWARF_TYPE_EH_FRAME_HDR);
        (*self)->dwarf_eh_frame_type = TDCD_DWARF_TYPE_EH_FRAME_HDR;
    }

    //dwarf from .eh_frame without .eh_frame_hdr
    if(NULL == (*self)->dwarf_eh_frame && 0 != (*self)->eh_frame_offset && 0 != (*self)->eh_frame_size)
    {
        tdcd_dwarf_create(&((*self)->dwarf_eh_frame), memory, pid,
                         (*self)->eh_frame_load_bias, 0,
                         (*self)->eh_frame_offset, (*self)->eh_frame_size,
                         TDCD_DWARF_TYPE_EH_FRAME);
        (*self)->dwarf_eh_frame_type = TDCD_DWARF_TYPE_EH_FRAME;
    }

    //dwarf from .debug_frame
    if(0 != (*self)->debug_frame_offset && 0 != (*self)->debug_frame_size)
        tdcd_dwarf_create(&((*self)->dwarf_debug_frame), memory, pid,
                         (*self)->load_bias, 0,
                         (*self)->debug_frame_offset, (*self)->debug_frame_size,
                         TDCD_DWARF_TYPE_DEBUG_FRAME);

    return 0;
}

tdcd_elf_interface_t *tdcd_elf_interface_gnu_create(tdcd_elf_interface_t *self)
{
    tdcd_elf_interface_t *gnu;
    tdcd_memory_t        *memory = NULL;
    uint8_t             *src = NULL;
    size_t               src_size;
    uint8_t             *dst = NULL;
    size_t               dst_size;
    
    if(0 == self->gnu_debugdata_offset || 0 == self->gnu_debugdata_size) return NULL;

    //dump xz data
    src_size = self->gnu_debugdata_size;
    if(NULL == (src = malloc(src_size))) goto err;
    if(0 != tdcd_memory_read_fully(self->memory, self->gnu_debugdata_offset, src, src_size)) goto err;

    //xz decompress
    if(0 != tdcd_util_xz_decompress(src, src_size, &dst, &dst_size)) goto err;

    //create memory object
    if(0 != tdcd_memory_create_from_buf(&memory, dst, dst_size)) goto err;

    //create ELF interface from .gnu_debugdata
    if(0 != tdcd_elf_interface_create(&gnu, self->pid, memory, NULL)) goto err;
    gnu->load_bias = self->load_bias;
    gnu->is_gnu = 1;

    return gnu;

 err:
    TDCD_LOG_WARN("ELF: create GNU interface FAILED");
    if(NULL != memory) tdcd_memory_destroy(&memory);
    if(NULL != dst) free(dst);
    if(NULL != src) free(src);
    return NULL;
}

int tdcd_elf_interface_dwarf_step(tdcd_elf_interface_t *self, uintptr_t step_pc, tdcd_regs_t *regs, int *finished)
{
    int r;

    //try .debug_frame
    if(NULL != self->dwarf_debug_frame)
    {
        r = tdcd_dwarf_step(self->dwarf_debug_frame, regs, step_pc, finished);
#if TDCD_ELF_INTERFACE_DEBUG
        TDCD_LOG_DEBUG("ELF: step by .debug_frame%s %s, step_pc=%"PRIxPTR", load_bias=%"PRIxPTR", finished=%d",
                      (self->is_gnu ? " (in .gnu_debugdata)" : ""),
                      (0 == r ? "OK" : "FAILED"),
                      step_pc, self->load_bias, *finished);
#endif
        if(0 == r) return 0;
    }

    //try .eh_frame (with or without .eh_frame_hdr)
    if(NULL != self->dwarf_eh_frame)
    {
        r = tdcd_dwarf_step(self->dwarf_eh_frame, regs, step_pc, finished);
#if TDCD_ELF_INTERFACE_DEBUG
        TDCD_LOG_DEBUG("ELF: step by %s%s %s, step_pc=%"PRIxPTR", load_bias=%"PRIxPTR", finished=%d",
                      (TDCD_DWARF_TYPE_EH_FRAME_HDR == self->dwarf_eh_frame_type ? ".eh_frame_hdr" : ".eh_frame"),
                      (self->is_gnu ? " (in .gnu_debugdata)" : ""),
                      (0 == r ? "OK" : "FAILED"),
                      step_pc, self->load_bias, *finished);
#endif
        if(0 == r) return 0;
    }
    
    return TDC_ERRNO_MISSING;
}

#ifdef __arm__
int tdcd_elf_interface_arm_exidx_step(tdcd_elf_interface_t *self, uintptr_t step_pc, tdcd_regs_t *regs, int *finished)
{
    int r;
    
    if(0 != self->arm_exidx_offset && 0 != self->arm_exidx_size)
    {
        r = tdcd_arm_exidx_step(regs, self->memory, self->pid, self->arm_exidx_offset, self->arm_exidx_size,
                                  self->load_bias, step_pc, finished);
#if TDCD_ELF_INTERFACE_DEBUG
        TDCD_LOG_DEBUG("ELF: step by .ARM.exidx %s, step_pc=%x, load_bias=%x, finished=%d",
                      (0 == r ? "OK" : "FAILED"), step_pc, self->load_bias, *finished);
#endif
        if(0 == r) return 0;
    }

    return TDC_ERRNO_MISSING;
}
#endif

int tdcd_elf_interface_get_function_info(tdcd_elf_interface_t *self, uintptr_t addr, char **name, size_t *name_offset)
{
    tdcd_elf_symbols_t *symbols;
    size_t             offset;
    size_t             start_offset;
    size_t             end_offset;
    size_t             str_offset;
    ElfW(Sym)          sym;
    char               buf[512];

    TAILQ_FOREACH(symbols, &(self->symbolsq), link)
    {
        for(offset = symbols->sym_offset; offset < symbols->sym_end; offset += symbols->sym_entry_size)
        {
            if(0 != tdcd_memory_read_fully(self->memory, offset, &sym, sizeof(sym))) break;
            if(sym.st_shndx == SHN_UNDEF || ELF_ST_TYPE(sym.st_info) != STT_FUNC) continue;
            
            start_offset = sym.st_value;
            end_offset = start_offset + sym.st_size;
            if(addr < start_offset || addr >= end_offset) continue;
            
            *name_offset = addr - start_offset;
            
            str_offset = symbols->str_offset + sym.st_name;
            if(str_offset >= symbols->str_end) continue;
            
            if(0 != tdcd_memory_read_string(self->memory, str_offset, buf, sizeof(buf), symbols->str_end - str_offset)) continue;
            if(NULL == (*name = strdup(buf))) break;

            return 0;
        }
    }

    *name = NULL;
    *name_offset = 0;
    return TDC_ERRNO_NOTFND;
}

int tdcd_elf_interface_get_symbol_addr(tdcd_elf_interface_t *self, const char *name, uintptr_t *addr)
{
    tdcd_elf_symbols_t *symbols;
    size_t             offset;
    size_t             str_offset;
    ElfW(Sym)          sym;
    char               buf[512];

    TAILQ_FOREACH(symbols, &(self->symbolsq), link)
    {
        for(offset = symbols->sym_offset; offset < symbols->sym_end; offset += symbols->sym_entry_size)
        {
            //read .symtab / .dynsym
            if(0 != tdcd_memory_read_fully(self->memory, offset, &sym, sizeof(sym))) break;
            if(sym.st_shndx == SHN_UNDEF) continue;

            //read .strtab / .dynstr
            str_offset = symbols->str_offset + sym.st_name;
            if(str_offset >= symbols->str_end) continue;
            if(0 != tdcd_memory_read_string(self->memory, str_offset, buf, sizeof(buf), symbols->str_end - str_offset)) continue;

            //compare symbol name
            if(0 != strcmp(name, buf)) continue;

            //found it
            *addr = sym.st_value;
            return 0;
        }
    }

    *addr = 0;
    return TDC_ERRNO_NOTFND;
}

int tdcd_elf_interface_get_build_id(tdcd_elf_interface_t *self, uint8_t *build_id, size_t build_id_len, size_t *build_id_len_ret)
{
    ElfW(Nhdr) nhdr;
    uintptr_t  addr;
    int        r;

    if(0 == self->build_id_offset || 0 == self->build_id_size) return TDC_ERRNO_MISSING;
    if(self->build_id_size < sizeof(nhdr)) return TDC_ERRNO_FORMAT;

    //read .note.gnu.build-id header
    if(0 != (r = tdcd_memory_read_fully(self->memory, self->build_id_offset, &nhdr, sizeof(nhdr)))) return r;
    if(0 == nhdr.n_descsz) return TDC_ERRNO_MISSING;
    if(nhdr.n_descsz > build_id_len) return TDC_ERRNO_NOSPACE;

    //read .note.gnu.build-id data
    addr = self->build_id_offset + sizeof(nhdr) + ((nhdr.n_namesz + 3) & (~(unsigned int)3)); //skip the name (should be "GNU\0": 0x47 0x4e 0x55 0x00)
    if(0 != (r = tdcd_memory_read_fully(self->memory, addr, build_id, nhdr.n_descsz))) return r;

    if(NULL != build_id_len_ret) *build_id_len_ret = nhdr.n_descsz;
    return 0;
}

char *tdcd_elf_interface_get_so_name(tdcd_elf_interface_t *self)
{
    uintptr_t         offset;
    ElfW(Dyn)         dyn;
    tdcd_elf_strtab_t *strtab;
    uintptr_t         strtab_addr = 0;
    uintptr_t         strtab_size = 0;
    uintptr_t         soname_offset = 0;
    char              buf[256] = "\0";

    if(0 == self->dynamic_offset || 0 == self->dynamic_size) goto err;
    if(NULL != self->so_name) return self->so_name;

    for(offset = self->dynamic_offset; offset < self->dynamic_offset + self->dynamic_size; offset += sizeof(dyn))
    {
        if(0 != tdcd_memory_read_fully(self->memory, offset, &dyn, sizeof(dyn))) goto err;
        if(DT_NULL == dyn.d_tag) break;
        switch(dyn.d_tag)
        {
        case DT_STRTAB:
            strtab_addr = dyn.d_un.d_ptr;
            break;
        case DT_STRSZ:
            strtab_size = (uintptr_t)(dyn.d_un.d_val);
            break;
        case DT_SONAME:
            soname_offset = (uintptr_t)(dyn.d_un.d_val);
            break;
        default:
            break;
        }
    }

    TAILQ_FOREACH(strtab, &(self->strtabq), link)
    {
        if(strtab->addr == strtab_addr)
        {
            soname_offset += strtab->offset;
            if(soname_offset >= strtab->offset + strtab_size) goto err;
            if(0 != tdcd_memory_read_string(self->memory, soname_offset, buf, sizeof(buf), strtab->offset + strtab_size - soname_offset)) goto err;
            if(NULL == (self->so_name = strdup(buf))) goto err;
            return self->so_name;
        }
    }

 err:
    self->so_name = "";
    return self->so_name;
}

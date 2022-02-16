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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../tdcrash/tdc_errno.h"
#include "tdc_dl.h"
#include "queue.h"
#include "../common/tdcc_util.h"
#include <android/log.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

typedef struct tdc_dl_symbols
{
    size_t sym_offset;
    size_t sym_end;
    size_t sym_entry_size;
    size_t str_offset;
    size_t str_end;
    TAILQ_ENTRY(tdc_dl_symbols,) link;
} tdc_dl_symbols_t;
typedef TAILQ_HEAD(tdc_dl_symbols_queue, tdc_dl_symbols,) tdc_dl_symbols_queue_t;

struct tdc_dl
{
    uintptr_t              map_start;
    int                    fd;
    uint8_t               *data;
    size_t                 size;
    uintptr_t              load_bias;
    tdc_dl_symbols_queue_t  symbolsq;
};

static int tdc_dl_find_map_start(tdc_dl_t *self, const char *pathname)
{
    FILE      *f = NULL;
    char       line[512];
    uintptr_t  offset;
    int        pos;
    char      *p;
    int        r = TDC_ERRNO_NOTFND;

    if(NULL == (f = fopen("/proc/self/maps", "r"))) return TDC_ERRNO_SYS;

    while(fgets(line, sizeof(line), f))
    {
        if(2 != sscanf(line, "%"SCNxPTR"-%*"SCNxPTR" %*4s %"SCNxPTR" %*x:%*x %*d%n", &(self->map_start), &offset, &pos)) continue;

        if(0 != offset) continue;
        p = tdcc_util_trim(line + pos);

        if(0 != strcmp(p, pathname)) continue;

        r = 0; //found
        break;
    }

    fclose(f);
    return r;
}

static int tdc_dl_file_open(tdc_dl_t *self, const char *pathname)
{
    struct stat st;

    //open file
    if(0 > (self->fd = TDCC_UTIL_TEMP_FAILURE_RETRY(open(pathname, O_RDONLY | O_CLOEXEC)))) return TDC_ERRNO_SYS;

    //get file size
    if(0 != fstat(self->fd, &st) || 0 == st.st_size) return TDC_ERRNO_SYS;
    self->size = (size_t)st.st_size;

    //mmap the file
    if(MAP_FAILED == (self->data = (uint8_t *)mmap(NULL, self->size, PROT_READ, MAP_PRIVATE, self->fd, 0))) return TDC_ERRNO_SYS;
    
    return 0;
}

static void *tdc_dl_file_get(tdc_dl_t *self, uintptr_t offset, size_t size)
{
    if(offset + size > self->size) return NULL;
    return (void *)(self->data + offset);
}

static char *tdc_dl_file_get_string(tdc_dl_t *self, uintptr_t offset)
{
    uint8_t *p = self->data + offset;
    
    while(p < self->data + self->size)
    {
        if('\0' == *p) return (char *)(self->data + offset);
        p++;
    }
    
    return NULL;
}

static int tdc_dl_parse_elf(tdc_dl_t *self)
{
    ElfW(Ehdr)      *ehdr;
    ElfW(Phdr)      *phdr;
    ElfW(Shdr)      *shdr, *str_shdr;
    tdc_dl_symbols_t *symbols;
    size_t           i, cnt = 0;
    
    //get ELF header
    if(NULL == (ehdr = tdc_dl_file_get(self, 0, sizeof(ElfW(Ehdr))))) return TDC_ERRNO_FORMAT;

    //find load_bias in program headers
    for(i = 0; i < ehdr->e_phnum * ehdr->e_phentsize; i += ehdr->e_phentsize)
    {
        if(NULL == (phdr = tdc_dl_file_get(self, ehdr->e_phoff + i, sizeof(ElfW(Phdr))))) return TDC_ERRNO_FORMAT;

        if((PT_LOAD == phdr->p_type) && (phdr->p_flags & PF_X) && (0 == phdr->p_offset))
        {
            self->load_bias = phdr->p_vaddr;
            break;
        }
    }

    //find symbol tables in section headers
    for(i = ehdr->e_shentsize; i < ehdr->e_shnum * ehdr->e_shentsize; i += ehdr->e_shentsize)
    {
        if(NULL == (shdr = tdc_dl_file_get(self, ehdr->e_shoff + i, sizeof(ElfW(Shdr))))) return TDC_ERRNO_FORMAT;

        if(SHT_SYMTAB == shdr->sh_type || SHT_DYNSYM == shdr->sh_type)
        {
            if(shdr->sh_link >= ehdr->e_shnum) continue;
            if(NULL == (str_shdr = tdc_dl_file_get(self, ehdr->e_shoff + shdr->sh_link * ehdr->e_shentsize, sizeof(ElfW(Shdr))))) return TDC_ERRNO_FORMAT;
            if(SHT_STRTAB != str_shdr->sh_type) continue;
            
            if(NULL == (symbols = malloc(sizeof(tdc_dl_symbols_t)))) return TDC_ERRNO_NOMEM;
            symbols->sym_offset = shdr->sh_offset;
            symbols->sym_end = shdr->sh_offset + shdr->sh_size;
            symbols->sym_entry_size = shdr->sh_entsize;
            symbols->str_offset = str_shdr->sh_offset;
            symbols->str_end = str_shdr->sh_offset + str_shdr->sh_size;
            TAILQ_INSERT_TAIL(&(self->symbolsq), symbols, link);
            cnt++;
        }
    }
    if(0 == cnt) return TDC_ERRNO_FORMAT;

    return 0;
}

tdc_dl_t *tdc_dl_create(const char *pathname)
{
    tdc_dl_t *self;

    if(NULL == (self = calloc(1, sizeof(tdc_dl_t)))) return NULL;
    self->fd = -1;
    self->data = MAP_FAILED;
    TAILQ_INIT(&(self->symbolsq));

    if(0 != tdc_dl_find_map_start(self, pathname)) goto err;
    if(0 != tdc_dl_file_open(self, pathname)) goto err;
    if(0 != tdc_dl_parse_elf(self)) goto err;

    return self;

 err:
    tdc_dl_destroy(&self);
    return NULL;
}

void tdc_dl_destroy(tdc_dl_t **self)
{
    tdc_dl_symbols_t *symbols, *symbols_tmp;
    
    if(NULL == self || NULL == *self) return;
    
    if(MAP_FAILED != (*self)->data) munmap((*self)->data, (*self)->size);
    
    if((*self)->fd >= 0) close((*self)->fd);
    
    TAILQ_FOREACH_SAFE(symbols, &((*self)->symbolsq), link, symbols_tmp)
    {
        TAILQ_REMOVE(&((*self)->symbolsq), symbols, link);
        free(symbols);
    }
    
    free(*self);
    *self = NULL;
}

void *tdc_dl_sym(tdc_dl_t *self, const char *symbol)
{
    tdc_dl_symbols_t *symbols;
    ElfW(Sym)       *sym;
    size_t           offset, str_offset;
    char            *str;

    TAILQ_FOREACH(symbols, &(self->symbolsq), link)
    {
        for(offset = symbols->sym_offset; offset < symbols->sym_end; offset += symbols->sym_entry_size)
        {
            //read .symtab / .dynsym
            if(NULL == (sym = tdc_dl_file_get(self, offset, sizeof(ElfW(Sym))))) break;
            if(SHN_UNDEF == sym->st_shndx) continue;

            //read .strtab / .dynstr
            str_offset = symbols->str_offset + sym->st_name;
            if(str_offset >= symbols->str_end) continue;
            if(NULL == (str = tdc_dl_file_get_string(self, str_offset))) continue;

            //compare symbol name
            if(0 != strcmp(symbol, str)) continue;

            //found
            return (void *)(self->map_start + sym->st_value - self->load_bias);
        }
    }
    
    return NULL;
}

#pragma clang diagnostic pop

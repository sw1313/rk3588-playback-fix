// SPDX-License-Identifier: MIT
#define _GNU_SOURCE
#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <link.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "csf_group_core.h"
static int (*original_close)(int);
static struct csf_groups groups;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static int (*original_ioctl)(int,unsigned long,...);
static int invoke(void *ctx,int fd,unsigned long request,void *arg) {
 (void)ctx;unsigned detail=0;
 if(arg && request==GROUP_CREATE)detail=((uint8_t*)arg)[24];
 if(arg && request==QUEUE_BIND)detail=((uint8_t*)arg)[9];
 int rc=original_ioctl(fd,request,arg);int error=errno;
 if(!rc && request==GROUP_CREATE)__android_log_print(ANDROID_LOG_INFO,"EdenRC3","CSF created group with cs_min=%u",detail);
 if(!rc && request==QUEUE_BIND)__android_log_print(ANDROID_LOG_INFO,"EdenRC3","CSF bound stream CSI%u",detail);
 errno=error;return rc<0?-error:rc;
}
static int grouped_close(int fd) {
 int saved=errno;pthread_mutex_lock(&mutex);csf_group_forget_fd(&groups,fd);
 pthread_mutex_unlock(&mutex);errno=saved;return original_close(fd);
}
static int grouped_ioctl(int fd,unsigned long request,...) {
 va_list ap;va_start(ap,request);void *arg=va_arg(ap,void*);va_end(ap);
 if(request!=GROUP_CREATE && request!=QUEUE_BIND && request!=GROUP_TERM)
   return original_ioctl(fd,request,arg);
 int saved=errno;pthread_mutex_lock(&mutex);
 int rc=csf_group_ioctl(&groups,(uint32_t)syscall(SYS_gettid),invoke,0,fd,request,arg);
 pthread_mutex_unlock(&mutex);if(rc<0){errno=-rc;return -1;}errno=saved;return rc;
}
struct slot { uintptr_t* target; uintptr_t before, after; };
struct scan {
    uintptr_t base, relro_start, relro_end;
    const Elf64_Phdr* phdr;
    size_t phnum, count;
    struct slot slots[3];
    int error;
};
static int readable(const struct scan* s, uintptr_t p, size_t size) {
    if (p + size < p) return 0;
    for (size_t i = 0; i < s->phnum; ++i) {
        const Elf64_Phdr* ph = &s->phdr[i];
        if (ph->p_type == PT_LOAD && (ph->p_flags & PF_R) &&
            p >= s->base + ph->p_vaddr && p + size <= s->base + ph->p_vaddr + ph->p_memsz)
            return 1;
    }
    return 0;
}
static uintptr_t dynaddr(const struct scan* s, uintptr_t p) {
    return readable(s, p, 1) ? p : s->base + p;
}
static int scan_payload(struct dl_phdr_info* info, size_t bytes, void* data) {
    (void)bytes;
    struct scan* s = data;
    if (info->dlpi_addr != s->base) return 0;
    s->phdr = info->dlpi_phdr; s->phnum = info->dlpi_phnum;
    const Elf64_Dyn* dynamic = NULL; size_t dynamic_size = 0;
    for (size_t i = 0; i < s->phnum; ++i) {
        const Elf64_Phdr* p = &s->phdr[i];
        if (p->p_type == PT_DYNAMIC) { dynamic = (void*)(s->base + p->p_vaddr); dynamic_size = p->p_memsz; }
        if (p->p_type == PT_GNU_RELRO) { s->relro_start = s->base + p->p_vaddr; s->relro_end = s->relro_start + p->p_memsz; }
    }
    if (!dynamic || !readable(s, (uintptr_t)dynamic, dynamic_size)) { s->error = 1; return 1; }
    const Elf64_Rela* relas = NULL; size_t rela_size = 0, str_size = 0;
    const Elf64_Sym* symbols = NULL; const char* strings = NULL;
    int rela_kind = 0;
    for (size_t i = 0; i < dynamic_size / sizeof(*dynamic) && dynamic[i].d_tag; ++i) {
        switch (dynamic[i].d_tag) {
            case DT_JMPREL: relas = (void*)dynaddr(s, dynamic[i].d_un.d_ptr); break;
            case DT_PLTRELSZ: rela_size = dynamic[i].d_un.d_val; break;
            case DT_PLTREL: rela_kind = dynamic[i].d_un.d_val; break;
            case DT_SYMTAB: symbols = (void*)dynaddr(s, dynamic[i].d_un.d_ptr); break;
            case DT_STRTAB: strings = (void*)dynaddr(s, dynamic[i].d_un.d_ptr); break;
            case DT_STRSZ: str_size = dynamic[i].d_un.d_val; break;
        }
    }
    if (!relas || !symbols || !strings || rela_kind != DT_RELA ||
        rela_size % sizeof(*relas) || !readable(s, (uintptr_t)relas, rela_size) ||
        !readable(s, (uintptr_t)strings, str_size)) { s->error = 2; return 1; }
    for (size_t i = 0; i < rela_size / sizeof(*relas); ++i) {
        if (ELF64_R_TYPE(relas[i].r_info) != R_AARCH64_JUMP_SLOT) continue;
        const Elf64_Sym* symbol = symbols + ELF64_R_SYM(relas[i].r_info);
        if (!readable(s, (uintptr_t)symbol, sizeof(*symbol)) || symbol->st_name >= str_size) { s->error = 3; return 1; }
        const char* name = strings + symbol->st_name;
        if (!memchr(name, 0, str_size - symbol->st_name)) { s->error = 4; return 1; }
        uintptr_t replacement = 0, expected = 0;
        if (!strcmp(name, "ioctl")) {
            if(relas[i].r_offset!=0x16933b8){s->error=7;return 1;}
            replacement = (uintptr_t)grouped_ioctl; expected = (uintptr_t)original_ioctl;
        }
        if (!strcmp(name, "close")) {
            if(relas[i].r_offset!=0x1693178){s->error=8;return 1;}
            replacement=(uintptr_t)grouped_close;expected=(uintptr_t)original_close;
        }
        if (!replacement) continue;
        uintptr_t address = s->base + relas[i].r_offset;
        if (s->count >= 3 || address % sizeof(uintptr_t) || !readable(s, address, sizeof(uintptr_t)) ||
            address < s->relro_start || address + sizeof(uintptr_t) > s->relro_end ||
            *(uintptr_t*)address != expected) { s->error = 5; return 1; }
        s->slots[s->count++] = (struct slot){(uintptr_t*)address, expected, replacement};
    }
    return 1;
}

int eden_install_group_adapter(void *payload) {
 void *marker=dlsym(payload,"HMI");Dl_info dl={0};
 if(!marker || !dladdr(marker,&dl))return -1;
 original_ioctl=dlsym(RTLD_DEFAULT,"ioctl");original_close=dlsym(RTLD_DEFAULT,"close");if(!original_ioctl || !original_close)return -2;
 struct scan s={.base=(uintptr_t)dl.dli_fbase};dl_iterate_phdr(scan_payload,&s);
 if(s.error || s.count!=2)return -10-s.error;
 const uint32_t expect[2]={0xf9401508,0x9b142003};
 uintptr_t code=s.base+0xe6e148;
 if(!readable(&s,code,sizeof(expect)) || memcmp((void*)code,expect,sizeof(expect)))return -30;

 long page=sysconf(_SC_PAGESIZE);if(page<=0 || (page&(page-1)))return -32;
 uintptr_t start=(uintptr_t)s.slots[0].target & ~(uintptr_t)(page-1);
 for(unsigned i=0;i<2;i++)if(((uintptr_t)s.slots[i].target & ~(uintptr_t)(page-1))!=start)return -31;
 if(mprotect((void*)start,(size_t)page,PROT_READ|PROT_WRITE))return -33;
 for(unsigned i=0;i<2;i++)__atomic_store_n(s.slots[i].target,s.slots[i].after,__ATOMIC_RELEASE);
 if(mprotect((void*)start,(size_t)page,PROT_READ)) {
  for(unsigned i=0;i<2;i++)*s.slots[i].target=s.slots[i].before;(void)mprotect((void*)start,(size_t)page,PROT_READ);return -34;
 }
 __android_log_print(ANDROID_LOG_INFO,"EdenRC3","Test8 CSG topology adapter: 3 CSIs per group, payload ioctl/close imports only");return 0;
}

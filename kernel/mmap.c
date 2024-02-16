#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "memlayout.h"
#include "mmap.h"

uint64
sys_mmap(void)
{
    uint64 addr;
    size_t len;
    int prot, flags, fd;
    off_t offset;
    argaddr(0, &addr);
    argaddr(1, &len);
    argint(2, &prot);
    argint(3, &flags);
    argint(4, &fd);
    argaddr(5, (uint64 *)&offset);
    return vma_map(myproc(), addr, len, prot, flags, fd, offset);
}
uint64
sys_munmap(void)
{
    uint64 addr;
    size_t len;
    argaddr(0, &addr);
    argaddr(1, &len);
    return vma_unmap(myproc(), addr, len);
}
int vma_access(struct proc *p, uint64 addr, int type)
{
    addr = PGROUNDDOWN(addr);
    // 1 for read, 2 for write
    for (int i = 0; i < VMA_COUNT; i++)
    {
        if (p->vma[i].len == 0)
            continue;
        if (addr < p->vma[i].addr)
            continue;
        if (addr >= p->vma[i].addr + p->vma[i].len)
            continue;
        if ((p->vma[i].prot & type) == 0)
        {
            return -1;
        }
        uint64 new_page = (uint64)kalloc();
        if (new_page == 0)
        {
            return -1;
        }
        if(mappages(p->pagetable, addr, PGSIZE, new_page, PTE_V | PTE_U | (p->vma[i].prot << 1)) != 0) {
            panic("vma_access(): mappages");
        }
        struct file * file = p->vma[i].fptr;
        ilock(file->ip);
        uint64 read_offset = addr - p->vma[i].file_base;
        uint64 read_end = p->vma[i].addr + p->vma[i].len;
        if(p->vma[i].file_base + file->ip->size < read_end) {
            read_end = p->vma[i].file_base + file->ip->size;
        }
        uint64 read_size = read_end - addr;
        if(PGSIZE < read_size) {
            read_size = PGSIZE;
        }
        if (readi(file->ip, 0, new_page, read_offset, read_size) != read_size)
        {
            panic("vma_access(): readi() failed");
        }
        if(read_size != PGSIZE) {
            memset((void *)(new_page + read_size), 0, PGSIZE - read_size);
        }
        iunlock(file->ip);
        return 0;
    }
    return -1;
}
int vma_copy(struct proc *old_proc, struct proc *new_proc)
{
    for (int i = 0; i < VMA_COUNT; i++)
    {
        if (new_proc->vma[i].len != 0)
        {
            panic("vma_copy(): copying into unempty vma_range struct");
        }
        if (old_proc->vma[i].len)
        {
            memmove(&new_proc->vma[i], &old_proc->vma[i], sizeof(struct vma_range));
            filedup(new_proc->vma[i].fptr);
        }
    }
    return 0;
}
int vma_free(struct proc *proc)
{
    for (int i = 0; i < VMA_COUNT; i++)
    {
        if (proc->vma[i].len)
        {
            vma_unmap_range(proc->pagetable, proc->vma[i].file_base, proc->vma[i].addr, proc->vma[i].len, (proc->vma[i].flags & MAP_SHARED) ? 1 : 0, proc->vma[i].fptr);
        }
    }
    memset(&proc->vma, 0, sizeof(proc->vma));
    return 0;
}
uint64 vma_map(struct proc *this_proc, uint64 start_addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    if (start_addr % PGSIZE != 0)
    {
        return -1;
    }
    if (len % PGSIZE != 0)
    {
        return -1;
    }
    if (start_addr != 0)
    {
        return -1;
    }
    if (offset != 0)
    {
        return -1;
    }
    if (len == 0)
    {
        return -1;
    }
    uint64 min_used = VMA_MAX;
    int free_vma = -1;
    for (int i = 0; i < VMA_COUNT; i++)
    {
        if (this_proc->vma[i].len != 0)
        {
            if (this_proc->vma[i].addr < min_used)
            {
                min_used = this_proc->vma[i].addr;
            }
        }
        else
        {
            free_vma = i;
        }
    }
    uint64 used_addr = PGROUNDDOWN(min_used - len);
    if (used_addr < VMA_MIN)
    {
        panic("vma_map(): no space to map!");
        return -1;
    }
    if (free_vma == -1)
    {
        return -1;
    }
    if (fd >= NOFILE)
    {
        return -1;
    }
    if (fd < 0)
    {
        return -1;
    }
    if (this_proc->ofile[fd] == 0)
    {
        panic("vma_map(): attempted to map to empty file descriptor");
        return -1;
    }
    if(this_proc->ofile[fd]->readable == 0 && prot & PROT_READ) {
        return -1;
    }
    if((flags & MAP_PRIVATE) == 0 && this_proc->ofile[fd]->writable == 0 && prot & PROT_WRITE) {
        return -1;
    }
    struct file *new_file = filedup(this_proc->ofile[fd]);
    if (new_file != this_proc->ofile[fd])
    {
        return -1;
    }
    this_proc->vma[free_vma].addr = used_addr;
    this_proc->vma[free_vma].len = len;
    this_proc->vma[free_vma].flags = flags;
    this_proc->vma[free_vma].prot = prot;
    this_proc->vma[free_vma].fptr = new_file;
    this_proc->vma[free_vma].file_base = used_addr;
    return used_addr;
}
int vma_unmap(struct proc *this_proc, uint64 start_addr, size_t len)
{
    if (len == 0)
    {
        return 0;
    }
    int vma_idx = -1;
    for (int i = 0; i < VMA_COUNT; i++)
    {
        if (this_proc->vma[i].len == 0)
            continue;
        if (start_addr >= this_proc->vma[i].addr + this_proc->vma[i].len)
            continue;
        if (start_addr + len <= this_proc->vma[i].addr)
            continue;
        if (vma_idx != -1)
        {
            panic("vma_unmap(): intersecting mappings");
        }
        vma_idx = i;
    }
    if (vma_idx == -1) {
        return -1;
    }
    uint64 new_addr;
    uint64 new_len;
    if (start_addr == this_proc->vma[vma_idx].addr)
    {
        new_len = this_proc->vma[vma_idx].len - len;
        new_addr = this_proc->vma[vma_idx].addr + len;
    }
    else if (start_addr + len == this_proc->vma[vma_idx].addr + this_proc->vma[vma_idx].len)
    {
        new_len = this_proc->vma[vma_idx].len - len;
        new_addr = this_proc->vma[vma_idx].addr;
    }
    else
    {
        return -1;
    }
    vma_unmap_range(this_proc->pagetable, this_proc->vma[vma_idx].file_base, start_addr, len, (this_proc->vma[vma_idx].flags & MAP_SHARED) ? 1 : 0, this_proc->vma[vma_idx].fptr);
    this_proc->vma[vma_idx].addr = new_addr;
    this_proc->vma[vma_idx].len = new_len;
    if (this_proc->vma[vma_idx].len == 0)
    {
        fileclose(this_proc->vma[vma_idx].fptr);
        memset(&this_proc->vma[vma_idx], 0, sizeof(struct vma_range));
    }
    return 0;
}
int vma_init(struct proc *proc)
{
    for (int i = 0; i < VMA_COUNT; i++)
    {
        if (proc->vma[i].len != 0)
        {
            panic("vma_init(): vma_range is not empty");
        }
    }
    return 0;
}
void vma_unmap_range(pagetable_t pagetable, uint64 base_addr, uint64 va, uint64 len, int writeback, struct file *file)
{
    uint64 a;
    pte_t *pte;

    if ((va % PGSIZE) != 0)
        panic("vma_unmap: not aligned");

    for (a = va; a < va + len; a += PGSIZE)
    {
        //printf("%p\n", a);
        if ((pte = walk(pagetable, a, 0)) == 0)
            continue;
        if((*pte & PTE_V) == 0) {
            continue;
        }
        //if ((*pte & PTE_V) == 0)
        //    panic("vma_unmap_range(): not mapped");
        if (PTE_FLAGS(*pte) == PTE_V)
            panic("vma_unmap_range(): not a leaf");
        uint64 pa = PTE2PA(*pte);
        if (writeback && *pte & PTE_D)
        {
            begin_op();
            ilock(file->ip);
            uint64 write_offset = a - base_addr;
            uint64 write_end = va + len;
            if(base_addr + file->ip->size < write_end) {
                write_end = base_addr + file->ip->size;
            }
            uint64 write_size = write_end - a;
            if(PGSIZE < write_size) {
                write_size = PGSIZE;
            }
            if (writei(file->ip, 0, pa, write_offset, write_size) != write_size)
            {
                panic("vma_unmap(): writei() failed");
            }
            iunlock(file->ip);
            end_op();
        }
        kfree((void *)pa);
        *pte = 0;
    }
}
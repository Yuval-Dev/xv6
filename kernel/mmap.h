#include "types.h"
#include "defs.h"

int vma_access(struct proc * p, uint64 addr, int type);
int vma_copy(struct proc * old_proc, struct proc * new_proc);
int vma_free(struct proc * proc);
uint64 vma_map(struct proc * this_proc, uint64 start_addr, size_t len, int prot, int flags, int fd, off_t offset);
int vma_unmap(struct proc * this_proc, uint64 start_addr, size_t len);
int vma_init(struct proc * proc);
void vma_unmap_range(pagetable_t pagetable, uint64 base_addr, uint64 addr, uint64 len, int writeback, struct file * file);
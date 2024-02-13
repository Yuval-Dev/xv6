#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
#define MAX_PAGES_SCANNED 1024
int
sys_pgaccess(void)
{
	uint64 base;
	int len;
	uint64 mask;
	argaddr(0, &base);
	argint(1, &len);
	argaddr(2, &mask);
	pagetable_t my_pagetable = myproc()->pagetable;
	if(len > MAX_PAGES_SCANNED) {
		return -1;
	}
	int bit_mask[MAX_PAGES_SCANNED / (sizeof(int) * 32)];
	memset(bit_mask, 0, sizeof(bit_mask));
	for(int idx = 0; idx < len; idx++) {
		uint64 cur_addr = base + idx * PGSIZE;
		pagetable_t cur_page = my_pagetable;
		for(int level = 2; level > 0; level--) {
			pte_t *pte = &cur_page[PX(level, cur_addr)];
			if(*pte & PTE_V) {
				cur_page = (pagetable_t)PTE2PA(*pte);
			} else {
				cur_page = 0;
				break;
			}
		}
		if(cur_page != 0) {
			pte_t *pte = &cur_page[PX(0, cur_addr)];
			if((*pte & PTE_V) == 0) {
				continue;
			}
			if((*pte & PTE_A) == 0) {
				continue;
			}
			*pte &= ~PTE_A;
			bit_mask[idx / (sizeof(int) * 8)] |= (1L << (idx % (sizeof(int) * 8)));
		}
	}
	//copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
	copyout(my_pagetable, mask, (char *)bit_mask, (len - 1) / 8 + 1);
	return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


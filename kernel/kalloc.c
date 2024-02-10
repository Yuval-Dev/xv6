// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define STEAL_MAX 16

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int amount_free;
} kmem[NCPU];

struct {
  struct spinlock lock;
} mem_steal;

void
kinit()
{
  initlock(&mem_steal.lock, "mem_steal");
  for(int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, "kmem");
    kmem[i].amount_free = 0;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  int cur_cpu;
  push_off();
  cur_cpu = cpuid();
  pop_off();
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cur_cpu].lock);
  r->next = kmem[cur_cpu].freelist;
  kmem[cur_cpu].freelist = r;
  kmem[cur_cpu].amount_free++;
  release(&kmem[cur_cpu].lock);
}


//steals pages from steal_cpu to cur_cpu
//assumes locks for both
void steal_from(int cur_cpu, int steal_cpu, int amount) {
  if(amount == 0) return;
  kmem[cur_cpu].amount_free += amount;
  kmem[steal_cpu].amount_free -= amount;
  struct run * top = kmem[steal_cpu].freelist;
  struct run * bot = top;
  for(int i = 0; i < amount - 1; i++) {
    bot = bot->next;
  }
  kmem[steal_cpu].freelist = bot->next;
  bot->next = kmem[cur_cpu].freelist;
  kmem[cur_cpu].freelist = top;
}

//steals enough pages to reach STEAL_MAX free pages
//assumes that a lock for the current kmem is already
//acquired
void steal_memory(int cur_cpu) {
  acquire(&mem_steal.lock);
  acquire(&kmem[cur_cpu].lock);
  for(int steal_cpu = 0; steal_cpu < NCPU && kmem[cur_cpu].amount_free < STEAL_MAX; steal_cpu++) {
    if(steal_cpu == cur_cpu) continue;
    acquire(&kmem[steal_cpu].lock);
    int steal_amount = STEAL_MAX - kmem[cur_cpu].amount_free;
    if(steal_amount > kmem[steal_cpu].amount_free) {
      steal_amount = kmem[steal_cpu].amount_free;
    }
    steal_from(cur_cpu, steal_cpu, steal_amount);
    release(&kmem[steal_cpu].lock);
  }
  release(&kmem[cur_cpu].lock);
  release(&mem_steal.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.

void *
kalloc(void)
{
  struct run *r;
  int cur_cpu;
  push_off();
  cur_cpu = cpuid();
  pop_off();

  if(kmem[cur_cpu].amount_free < STEAL_MAX) {
    steal_memory(cur_cpu);
  }
  acquire(&kmem[cur_cpu].lock);
  r = kmem[cur_cpu].freelist;
  if(r) {
    kmem[cur_cpu].freelist = r->next;
    kmem[cur_cpu].amount_free--;
  }
  release(&kmem[cur_cpu].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
// defined by kernel.ld.

//potential change: change type to char
unsigned int ref_count[PHYSTOP / PGSIZE];

struct run {
	struct run *next;
};

struct {
	struct spinlock lock;
	struct run *freelist;
} kmem;

uint64 get_freemem(void) {
 struct run *r;
  uint64 amount = 0;

  acquire(&kmem.lock);
  r = kmem.freelist;
  while(r) {
    r = r->next;
    amount += PGSIZE;
  }

  release(&kmem.lock);

  return amount;	
}

int 
copy_on_write(pte_t * pte) {
	int flags;
	uint64 cur_phys_addr;
	uint64 new_phys_addr;
	//uint64 cur_page_index;

	flags = PTE_FLAGS(*pte);

	//the page isn't even COW, exit with error
	if((flags & PTE_COW) == 0)
		return -1;

	cur_phys_addr = PTE2PA(*pte);
	
	//cur_page_index = cur_phys_addr / PGSIZE;
	/*
	acquire(&kmem.lock);
	if(ref_count[cur_page_index] == 1) {
		//this is the last reference to the page, so there is no need to copy,
		//we can simply set the page to write and disable cow, since the page is under
		//sole ownership of the current process
		*pte |= PTE_W;
		*pte &= ~PTE_COW;
		release(&kmem.lock);
		return 0;
	}
	release(&kmem.lock);
	*/
	//if ref count is greater than 1, then we need to decrement the counter, and copy.
	//lets save the reference count decrementing till the end, so that we are sure that
	//the copy succeeded fully before decrementing the counter, this way we prevent
	//dangling page references
	
	new_phys_addr = (uint64)kalloc();

	if(new_phys_addr == 0)
		return -2;

	//the copy part of the COW
	memmove((void *)new_phys_addr, (char *)cur_phys_addr, PGSIZE);

	//set the pte to the copied page
	*pte = PA2PTE(new_phys_addr) | PTE_FLAGS(*pte) | PTE_W;
	*pte &= ~PTE_COW;

	//finally, lets not forget to decrement the old page's ref count
	
	kfree((void *)cur_phys_addr);
	//acquire(&kmem.lock);
	//ref_count[cur_page_index]--;
	//release(&kmem.lock);
	return 0;
}
	void
kinit()
{
	for(int i = 0 / PGSIZE; i < PHYSTOP / PGSIZE; i++) {
		ref_count[i] = 1;
	}
	initlock(&kmem.lock, "kmem");
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

	int page_index;
	struct run *r;

	if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
		panic("kfree");

	page_index = (uint64)pa / PGSIZE;
	acquire(&kmem.lock);
	ref_count[page_index]--;
	if(ref_count[page_index] == 0) {
		// Fill with junk to catch dangling refs.
		memset(pa, 1, PGSIZE);

		r = (struct run*)pa;

		r->next = kmem.freelist;
		kmem.freelist = r;
	}
	release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
	void *
kalloc(void)
{
	int page_index;
	struct run *r;
	
	acquire(&kmem.lock);
	r = kmem.freelist;
	if(r)
		kmem.freelist = r->next;

	if(r)
		memset((char*)r, 5, PGSIZE); // fill with junk

	page_index = (uint64)r / PGSIZE;
	ref_count[page_index] = 1;
	release(&kmem.lock);
	return (void*)r;
}

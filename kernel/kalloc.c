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

void ppnref_init();

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

extern uint8 ppn_ref[];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct spinlock ppn_ref_lock;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ppn_ref_lock, "ppn_ref");
  freerange(end, (void*)PHYSTOP);
  ppnref_init();
}

void ppnref_init()
{
  for (uint32 i = 0; i < PPNRANGE; i++) {
    ppn_ref[i] = 0;
  }
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
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Before freeing, check the ppn_ref
  acquire(&ppn_ref_lock);
  if (ppn_ref[((uint64)pa-(uint64)end)>>12] > 0) {
    ppn_ref[((uint64)pa-(uint64)end)>>12] -= 1;
    release(&ppn_ref_lock);
    return;
  }
  release(&ppn_ref_lock);

  // ppn_ref[((uint64)pa - (uint64)end)>>12] equals to 0, should be freed
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

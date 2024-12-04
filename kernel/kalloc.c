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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct { //reference count struct for pages (for COW)
  struct spinlock lock;
  int count[(PGROUNDUP(PHYSTOP) - KERNBASE) / PGSIZE];
} refcnt;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock (&refcnt.lock, "refcnt");

  for(int i=0; i<(PGROUNDUP(PHYSTOP) - KERNBASE) / PGSIZE; i++){
    refcnt.count[i] = 1; //initialize all page reference counts to 1
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
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if (ref_get(pa) <= 0) 
    panic("kfree: decrementing ref count already 0");

  ref_decr(pa); // decrement reference count for the page being freed
  if(ref_get(pa) > 0) // if the reference count is still greater than 0, child still alive, don't free the page
    return;
    
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

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    ref_incr(r);
  }
  return (void*)r;
}

// increment reference count for a page
void
ref_incr(void *pa)
{
  acquire(&refcnt.lock);
  refcnt.count[PA2IDX(pa)]++;
  release(&refcnt.lock);
}

// decrement reference count for a page
void
ref_decr(void *pa)
{
  acquire(&refcnt.lock);
  refcnt.count[PA2IDX(pa)]--;
  release(&refcnt.lock);
}

// get reference count for a page
int
ref_get(void *pa)
{
  int count;
  acquire(&refcnt.lock);
  count = refcnt.count[PA2IDX(pa)];
  release(&refcnt.lock);
  //printf("ref count for page %p is %d\n", pa, count);
  return count;
}

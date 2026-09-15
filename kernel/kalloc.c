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

struct {
  struct spinlock lock;
  uint8 cowcount[(PHYSTOP - KERNBASE) / PGSIZE];
} kcow;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kcow.lock, "kcow");
  memset(&kcow.cowcount, 1, (PHYSTOP - KERNBASE) / PGSIZE);
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

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  r = (struct run*)pa;

  acquire(&kmem.lock);
  acquire(&kcow.lock);
  uint8 cowcnt = kcow.cowcount[((uint64)pa - KERNBASE) / PGSIZE];
  if(cowcnt == 0){
    panic("kfree: refcnt");
  }
  kcow.cowcount[((uint64)pa - KERNBASE) / PGSIZE] --;
  cowcnt = kcow.cowcount[((uint64)pa - KERNBASE) / PGSIZE];
  if(cowcnt == 0){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&kcow.lock);
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
  if(r){
    kmem.freelist = r->next;
    acquire(&kcow.lock);
    uint64 pa = (uint64)r;
    kcow.cowcount[(pa - KERNBASE) / PGSIZE] = 1;
    release(&kcow.lock);
  }
  release(&kmem.lock);



  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
kaddref(uint64 pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kaddref");

  acquire(&kcow.lock);
  if(kcow.cowcount[(pa - KERNBASE) / PGSIZE] == 0){
    panic("kaddref:wrong cnt");
  }
  kcow.cowcount[(pa - KERNBASE) / PGSIZE]++;
  release(&kcow.lock);
}

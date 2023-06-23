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
  int cnt[PHYSTOP / PGSIZE];
} ref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref");
  freerange(end, (void*)PHYSTOP);
}

void
refinc(void* pa) {
  // if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
  //   return -1;
  acquire(&ref.lock);
  ++ref.cnt[(uint64)pa / PGSIZE];
  release(&ref.lock);
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

  acquire(&ref.lock);
  if(--ref.cnt[(uint64)pa / PGSIZE] == 0) {
    // Fill with junk to catch dangling refs. 装满垃圾以捕捉悬挂的参考。
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&ref.lock);
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

  if(r) {
    kmem.freelist = r->next;
    acquire(&ref.lock);
    ref.cnt[(uint64)r / PGSIZE] = 1;
    release(&ref.lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

int
cowkalloc(pagetable_t pagetable, uint64 va) {
  uint64 pa;
  pte_t *pte;
  uint flags;
  char* mem;
  if(va > MAXVA) {
    return -1;
  }
  va = PGROUNDDOWN(va);
  pte = walk(pagetable, va, 0);
  pa = PTE2PA(*pte);
  flags = PTE_FLAGS(*pte);
  if(flags & PTE_C) {
    if((mem = kalloc()) == 0) {
      return -1;;
    }
    memmove(mem, (char*)pa, PGSIZE);
    flags |= PTE_W;
    flags &= ~PTE_C;
    if(mappages(pagetable, va, PGSIZE, (uint64)mem, flags) != 0) {
      kfree(mem);
      return -1;
    }
  }
  return 0;
}

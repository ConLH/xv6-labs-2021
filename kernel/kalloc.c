// Physical memory allocator, for user processes, 物理内存分配器，用于用户进程
// kernel stacks, page-table pages, 内核栈，页表页
// and pipe buffers. Allocates whole 4096-byte pages. 管道缓冲区。 分配整个 4096 字节的页面

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

void
kinit()
{
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory. 分配一个 4096 字节的物理内存页。
// Returns a pointer that the kernel can use. 返回内核可以使用的指针。
// Returns 0 if the memory cannot be allocated. 如果无法分配内存，则返回 0
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
    memset((char*)r, 5, PGSIZE); // fill with junk 填满垃圾
  return (void*)r;
}

uint64
freememsize(void) 
{
  struct run *begin;
  uint64 size = 0;
  int cnt = 0;

  acquire(&kmem.lock);
  begin = kmem.freelist;
  while(begin) {
    begin = begin -> next;
    cnt++;
  }
  release(&kmem.lock);
  size = cnt * PGSIZE;
  return size;
}

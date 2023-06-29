// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;

#define NBUCKET 13
#define HASH(id) (id % NBUCKET)

struct hashbuf {
  struct buf head;
  struct spinlock lock;
};

struct {
  struct buf buf[NBUF];
  struct hashbuf buckets[NBUCKET];
} bcache;

// void
// binit(void)
// {
//   struct buf *b;

//   initlock(&bcache.lock, "bcache");

//   // Create linked list of buffers
//   bcache.head.prev = &bcache.head;
//   bcache.head.next = &bcache.head;
//   for(b = bcache.buf; b < bcache.buf+NBUF; b++){
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     initsleeplock(&b->lock, "buffer");
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
// }

void
binit(void) {
  struct buf *b;
  char lockname[16];

  for(int i = 0; i < NBUCKET; ++i) {
    snprintf(lockname, sizeof(lockname), "bcachelock_%d", i);
    initlock(&bcache.buckets[i].lock, lockname);
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }
  for(b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev. 查看缓冲区缓存以查找设备开发上的块。
// If not found, allocate a buffer. 如果没有找到，则分配一个缓冲区。
// In either case, return locked buffer. 无论哪种情况，都返回锁定的缓冲区。
// static struct buf*
// bget(uint dev, uint blockno)
// {
//   struct buf *b;

//   acquire(&bcache.lock);

//   // Is the block already cached?
//   for(b = bcache.head.next; b != &bcache.head; b = b->next){
//     if(b->dev == dev && b->blockno == blockno){
//       b->refcnt++;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }

//   // Not cached.
//   // Recycle the least recently used (LRU) unused buffer.
//   for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//     if(b->refcnt == 0) {
//       b->dev = dev;
//       b->blockno = blockno;
//       b->valid = 0;
//       b->refcnt = 1;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }
//   panic("bget: no buffers");
// }
static struct buf*
bget(uint dev, uint blockno) {
  struct buf *b;
  uint id = HASH(blockno);
  acquire(&bcache.buckets[id].lock);
  for(b = bcache.buckets[id].head.next; b != &bcache.buckets[id].head; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      release(&bcache.buckets[id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  b = 0;
  struct buf *tmp;
  for(int i = id, cycle = 0; cycle < NBUCKET; i = (i + 1) % NBUCKET, cycle++) {
    // if(!holding(&bcache.buckets[i].lock)) {
    //   acquire(&bcache.buckets[i].lock);
    // }
    if(i != id) {
      if(!holding(&bcache.buckets[i].lock)) {
        acquire(&bcache.buckets[i].lock);
      }else {
        continue;
      }
    }
    for(tmp = bcache.buckets[i].head.next; tmp != &bcache.buckets[i].head; tmp = tmp->next) {
      if(tmp->refcnt == 0 && (b == 0 || b->timestamp > tmp->timestamp)) {
        b = tmp;
      }
    }
    if(b) {
      if(i != id) {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.buckets[i].lock);
        b->next = bcache.buckets[id].head.next;
        b->prev = &bcache.buckets[id].head;
        bcache.buckets[id].head.next->prev = b;
        bcache.buckets[id].head.next = b;
      }
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      release(&bcache.buckets[id].lock);
      acquiresleep(&b->lock);
      return b;
    }else {
      if(i != id) {
        release(&bcache.buckets[i].lock);
      }
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf* //返回一个锁定的缓冲区，其中包含指定块的内容。
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
// void
// brelse(struct buf *b)
// {
//   if(!holdingsleep(&b->lock))
//     panic("brelse");

//   releasesleep(&b->lock);

//   acquire(&bcache.lock);
//   b->refcnt--;
//   if (b->refcnt == 0) {
//     // no one is waiting for it.
//     b->next->prev = b->prev;
//     b->prev->next = b->next;
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
  
//   release(&bcache.lock);
// }
void
brelse(struct buf *b) {
  if(!holdingsleep(&b->lock))
    panic("brelse");
  int id = HASH(b->blockno);
  releasesleep(&b->lock);
  acquire(&bcache.buckets[id].lock);
  b->refcnt--;
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);
  release(&bcache.buckets[id].lock);
}

void
bpin(struct buf *b) {
  int id = HASH(b->blockno);
  acquire(&bcache.buckets[id].lock);
  b->refcnt++;
  release(&bcache.buckets[id].lock);
}

void
bunpin(struct buf *b) {
  int id = HASH(b->blockno);
  acquire(&bcache.buckets[id].lock);
  b->refcnt--;
  release(&bcache.buckets[id].lock);
}



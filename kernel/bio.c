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

#define NBUCKETS 13
unsigned int hash(unsigned int x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x % NBUCKETS;
}

struct {
  struct buf buf[NBUF];
  struct {
    struct spinlock lock;
    struct buf head;
  } buckets[NBUCKETS];
  struct spinlock lock;
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  for(int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.buckets[i].lock, "bcache.bucket");
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }
  int i = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[i].head.next;
    b->prev = &bcache.buckets[i].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[i].head.next->prev = b;
    bcache.buckets[i].head.next = b;
    i++;
    if(i == NBUCKETS) i =0;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  unsigned int block_hash = hash(blockno);

  acquire(&bcache.buckets[block_hash].lock);

  // Is the block already cached?
  for(b = bcache.buckets[block_hash].head.next; b != &bcache.buckets[block_hash].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[block_hash].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(int i = 0; i < NBUCKETS; i++) {
    int bucket = (block_hash + i) % NBUCKETS;
    if(bcache.buckets[bucket].head.next->refcnt || bcache.buckets[bucket].head.next == &bcache.buckets[bucket].head) continue;
    if(i == 0) {
      b = bcache.buckets[bucket].head.next;
      b->next->prev = b->prev;
      b->prev->next = b->next;
    } else{
      acquire(&bcache.buckets[bucket].lock);
      if(bcache.buckets[bucket].head.next->refcnt || bcache.buckets[bucket].head.next == &bcache.buckets[bucket].head) {
        release(&bcache.buckets[bucket].lock);
        continue;
      }
      b = bcache.buckets[bucket].head.next;
      b->next->prev = b->prev;
      b->prev->next = b->next;
      release(&bcache.buckets[bucket].lock);
    }
    b->prev = bcache.buckets[block_hash].head.prev;
    b->next = &bcache.buckets[block_hash].head;
    bcache.buckets[block_hash].head.prev->next = b;
    bcache.buckets[block_hash].head.prev = b;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.buckets[block_hash].lock);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.buckets[block_hash].lock);
  panic("bget: no buffersa");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
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
void
brelse(struct buf *b)
{
  unsigned int block_hash = hash(b->blockno);
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.buckets[block_hash].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.buckets[block_hash].head.next;
    b->prev = &bcache.buckets[block_hash].head;
    bcache.buckets[block_hash].head.next->prev = b;
    bcache.buckets[block_hash].head.next = b;
  }
  release(&bcache.buckets[block_hash].lock);
}

void
bpin(struct buf *b) {
  unsigned int block_hash = hash(b->blockno);
  acquire(&bcache.buckets[block_hash].lock);
  b->refcnt++;
  release(&bcache.buckets[block_hash].lock);
}

void
bunpin(struct buf *b) {
  unsigned int block_hash = hash(b->blockno);
  acquire(&bcache.buckets[block_hash].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.buckets[block_hash].head.next;
    b->prev = &bcache.buckets[block_hash].head;
    bcache.buckets[block_hash].head.next->prev = b;
    bcache.buckets[block_hash].head.next = b;
  }
  release(&bcache.buckets[block_hash].lock);
}



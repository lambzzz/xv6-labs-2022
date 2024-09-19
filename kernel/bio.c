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
#define HASH(x) ((x) % NBUCKETS)

struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  for (int i = 0; i < NBUCKETS; i++)
  {
    initlock(&bcache.lock[i], "bcache");
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  for (b = bcache.buf; b < bcache.buf+NBUF; b++)
  {
    b->next = bcache.head[HASH(b->blockno)].next;
    b->prev = &bcache.head[HASH(b->blockno)];
    initsleeplock(&b->lock, "buffer");
    bcache.head[HASH(b->blockno)].next->prev = b;
    bcache.head[HASH(b->blockno)].next = b;
  }
  
  
  // initlock(&bcache.lock, "bcache");

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucketid = HASH(blockno);

  acquire(&bcache.lock[bucketid]);

  // Is the block already cached?
  for(b = bcache.head[bucketid].next; b != &bcache.head[bucketid]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[bucketid]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head[bucketid].prev; b != &bcache.head[bucketid]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[bucketid]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.lock[bucketid]);
  for (int i = 1; i < NBUCKETS; i++)
  {
    int other_bucket = (bucketid + i) % NBUCKETS;
    acquire(&bcache.lock[other_bucket]);
    for(b = bcache.head[other_bucket].prev; b != &bcache.head[other_bucket]; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // remove from the list
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.lock[other_bucket]);

        // add to the head
        acquire(&bcache.lock[bucketid]);
        b->next = bcache.head[bucketid].next;
        b->prev = &bcache.head[bucketid];
        bcache.head[bucketid].next->prev = b;
        bcache.head[bucketid].next = b;
        release(&bcache.lock[bucketid]);

        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[other_bucket]);
  }
  panic("bget: no buffers");
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
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int bucketid = HASH(b->blockno);
  releasesleep(&b->lock);

  acquire(&bcache.lock[bucketid]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[bucketid].next;
    b->prev = &bcache.head[bucketid];
    bcache.head[bucketid].next->prev = b;
    bcache.head[bucketid].next = b;
  }
  
  release(&bcache.lock[bucketid]);
}

void
bpin(struct buf *b) {
  int bucketid = HASH(b->blockno);
  acquire(&bcache.lock[bucketid]);
  b->refcnt++;
  release(&bcache.lock[bucketid]);
}

void
bunpin(struct buf *b) {
  int bucketid = HASH(b->blockno);
  acquire(&bcache.lock[bucketid]);
  b->refcnt--;
  release(&bcache.lock[bucketid]);
}



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

#define BUCKET 13

struct spinlock biglock;
struct buf buf[NBUF];
struct {
  struct spinlock lock;
  struct buf *e[NBUF];
  int n;
} bucket[BUCKET];


static int hash(uint blockno){
  return blockno % BUCKET;
}

static uint getticks(){
  return ticks + 1;
}//distinguish from 0


static struct buf* blookup(uint dev, uint blockno);

void
binit(void)
{
  struct buf *b;
  int i;
  
  initlock(&biglock, "bcache.big");
  for(i = 0;i < BUCKET; i++){
    initlock(&bucket[i].lock, "bcache.bucket");
    bucket[i].n = 0;
  }

  for(b = buf; b < buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->lastuse = 0;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucketid, n, mintickblockid, i, bufid;
  uint mintick, tries = 0;
  // Is the block already cached?
  bucketid = hash(blockno);
  acquire(&bucket[bucketid].lock);
  if((b = blookup(dev, blockno))){
    b->refcnt++;
    b->lastuse = getticks();
    release(&bucket[bucketid].lock);
    acquiresleep(&b->lock);
    return b;
  }
  
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  release(&bucket[bucketid].lock);
  
  acquire(&biglock);
  acquire(&bucket[bucketid].lock);

  if((b = blookup(dev, blockno))){
    b->refcnt++;
    b->lastuse = getticks();
    release(&bucket[bucketid].lock);
    release(&biglock);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bucket[bucketid].lock);

 retry:
  tries++;
  if(tries > 10000)
    panic("bget: too many retry");
  bufid = -1;
  mintick = -1;
  for(i = 0; i < NBUF; i++){
    b = &buf[i];
    // no locks need. now no body can eliminate block except this. 
    if(mintick > b -> lastuse && b -> refcnt == 0){
      mintick = b -> lastuse;
      mintickblockid = hash(b -> blockno);
      bufid = i;
    }
  }
  if(bufid < 0){
    release(&biglock);
    panic("bget: no buffers");
  }
  acquire(&bucket[mintickblockid].lock);
  n = bucket[mintickblockid].n;
  for(i = 0;i < n;i++){
    b = bucket[mintickblockid].e[i];
    // find block
    if(bucket[mintickblockid].e[i] == &buf[bufid]){
      // delete old
      if(b -> refcnt == 0){
        bucket[mintickblockid].e[i] = bucket[mintickblockid].e[--n];
        bucket[mintickblockid].n = n;
        break;
      }else{
        release(&bucket[mintickblockid].lock);
        goto retry;
      }
    }
  }
  release(&bucket[mintickblockid].lock);

  acquire(&bucket[bucketid].lock);
  n = bucket[bucketid].n;
  buf[bufid].dev = dev;
  buf[bufid].blockno = blockno;
  buf[bufid].valid = 0;
  buf[bufid].refcnt = 1;
  buf[bufid].lastuse = getticks();
  bucket[bucketid].e[n++] = &buf[bufid];
  bucket[bucketid].n = n;
  release(&bucket[bucketid].lock);
  release(&biglock);
  b = &buf[bufid];
  acquiresleep(&b->lock);
  return b;
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
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int blockno = b->blockno;
  int bucketid = hash(blockno); // caller must have refcnt.
  acquire(&bucket[bucketid].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b -> lastuse = getticks();
  }
  
  release(&bucket[bucketid].lock);
}

void
bpin(struct buf *b) {
  int blockno = b->blockno;
  int bucketid = hash(blockno); // caller must have refcnt.
  acquire(&bucket[bucketid].lock);
  b->refcnt++;
  release(&bucket[bucketid].lock);
}

void
bunpin(struct buf *b) {
  int blockno = b->blockno;
  int bucketid = hash(blockno); // caller must have refcnt.
  acquire(&bucket[bucketid].lock);
  b->refcnt--;
  release(&bucket[bucketid].lock);
}

// caller must hold bucket[h].lock
static struct buf*
blookup(uint dev, uint blockno)
{
  int h = hash(blockno);
  int i;
  for(i = 0; i < bucket[h].n; i++){
    struct buf *b = bucket[h].e[i];
    if(b->dev == dev && b->blockno == blockno)
      return b;
  }
  return 0;
}

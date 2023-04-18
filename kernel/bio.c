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
#include "proc.h"

struct spinlock replace_lock;
struct spinlock test_lock;

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

struct {
  // buf_map lock is actually a substitute for bcache.lock;
  // and always remember this
  struct spinlock lock;
  struct buf_entry *head;
} buf_map[BCMAPSIZE]; // blockno to bcache idx

void rm_buf_entry(uint idx, uint buf_idx);
void add_buf_entry(uint idx, uint buf_idx);

inline uint
bucket_idx(uint dev, uint blockno) {
  return ((dev << 27) + blockno) % BCMAPSIZE;
}

// this func should return without any lock
struct buf_entry *
bcfind(uint dev, uint blockno) {

  // idx here means idx to find
  uint idx = bucket_idx(dev, blockno);
  printf("ac bufmap idx\nnoff=%d\n", mycpu()->noff);

  // one process can change the structure of buf_map bucket
  // only when holding buf_map[idx].lock and replace_lock;
  // so lock buf_map[idx].lock to protect the bucket
  acquire(&buf_map[idx].lock);

  struct buf_entry *ret = buf_map[idx].head;
  while (ret) {
    if (bcache.buf[ret->buf_idx].blockno == blockno &&
        bcache.buf[ret->buf_idx].dev == dev) {

      // refcnt++ in order to avoid buf being evicted
      // release spinlock to acquire sleeplock safely
      bcache.buf[ret->buf_idx].refcnt++;
      release(&buf_map[idx].lock);
      return ret;
    } else {
      ret = ret->next;
    }
  }

  // not found; release bucket lock first
  release(&buf_map[idx].lock);
  acquire(&replace_lock);

  // check if other processes have finished the eviction
  // when holding replace_lock, the structure of buckets
  // will not changed by other processes, so don't need
  // the buf_map[idx].lock here
  ret = buf_map[idx].head;
  while (ret) {
    if (bcache.buf[ret->buf_idx].blockno == blockno &&
        bcache.buf[ret->buf_idx].dev == dev) {

      // replace_lock only protects the structure of buckets, so
      // we should acquire buf_map[idx].lock before refcnt++
      acquire(&buf_map[idx].lock);
      bcache.buf[ret->buf_idx].refcnt++;
      release(&buf_map[idx].lock);
      release(&replace_lock);
      return ret;
    } else {
      ret = ret->next;
    }
  }

  // do eviction
  // we are here with replace_lock holded only
  // and only this process can change the structure of buckets
  uint ev_idx, ev_bfidx;
  for (ev_bfidx = 0; ev_bfidx < NBUF; ev_bfidx++) {
    ev_bfidx %= NBUF;
    uint i = ev_bfidx;
    ev_idx = bucket_idx(bcache.buf[i].dev, bcache.buf[i].blockno);

    acquire(&buf_map[ev_idx].lock);
    if (bcache.buf[i].refcnt == 0) {
      break;
    }
    release(&buf_map[ev_idx].lock);
  }

  // have found the buf to be evicted
  // holding the replace_lock and buf_map[evidx].lock
  // but we should also change bucet buf_map[idx]
  // be careful because idx may equal to evidx
  if (ev_idx != idx) {
    acquire(&buf_map[idx].lock);
  }

  rm_buf_entry(ev_idx, ev_bfidx);
  struct buf *b = bcache.buf + ev_bfidx;
  b->blockno = blockno;
  b->dev = dev;
  b->valid = 0;
  b->refcnt = 1;
  add_buf_entry(idx, ev_bfidx);


  if(ev_idx != idx)
    release(&buf_map[idx].lock);
  release(&buf_map[ev_idx].lock);
  release(&replace_lock);
  // all lock released

  return &b->buf_entry;
}

void
rm_buf_entry(uint idx, uint buf_idx) {
  // You should hold bc_map[idx].lock
  struct buf_entry *prev = buf_map[idx].head;

  if (prev->buf_idx == buf_idx) {
    buf_map[idx].head = prev->next;
    return;
  }

  struct buf_entry *tmp = prev->next;
  while (tmp && tmp->buf_idx != buf_idx) {
    prev = tmp;
    tmp = tmp->next;
  }

  if (!tmp) {
    panic("rmbcentry");
  }

  prev->next = tmp->next;
}

void
add_buf_entry(uint idx, uint buf_idx) {
  // You should hold bc_hashtable[idx].lock
  bcache.buf[buf_idx].buf_entry.next = buf_map[idx].head;
  buf_map[idx].head = &bcache.buf[buf_idx].buf_entry;
}

void
binit(void)
{
  struct buf *b;

  initlock(&replace_lock, "replace_lock");

  // init bc map
  for (int i = 0; i < BCMAPSIZE; i++) {
    buf_map[i].head = 0;
    initlock(&buf_map[i].lock, "buf_map");
  }

  for (int i = 0; i < NBUF; i++) {
    b = bcache.buf + i;
    b->blockno = 0;
    b->buf_entry.buf_idx = i;
    add_buf_entry(0, i);
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf_entry *buf_entry = bcfind(dev, blockno);
  // we should acquire the sleep lock of buf
  // before bread and bwrite
  acquiresleep(&bcache.buf[buf_entry->buf_idx].lock);
  struct buf *b = bcache.buf + buf_entry->buf_idx;
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
// Should release buf lock
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  b->refcnt--;
  releasesleep(&b->lock);
}

void
bpin(struct buf *b) {
  uint idx = bucket_idx(b->dev, b->blockno);
  acquire(&buf_map[idx].lock);
  b->refcnt++;
  release(&buf_map[idx].lock);
}

void
bunpin(struct buf *b) {
  uint idx = bucket_idx(b->dev, b->blockno);
  acquire(&buf_map[idx].lock);
  b->refcnt--;
  release(&buf_map[idx].lock);
}



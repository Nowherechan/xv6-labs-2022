struct buf_entry {
  uint buf_idx;
  struct buf_entry *next;
};

struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *next;
  uchar data[BSIZE];

  struct buf_entry buf_entry;
};


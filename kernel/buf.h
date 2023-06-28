struct buf {
  int valid;   // has data been read from disk? 缓冲区是否包含块的副本
  int disk;    // does disk "own" buf? 缓冲区内容是否已经交给磁盘
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};


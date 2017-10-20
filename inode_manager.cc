#include "inode_manager.h"
#define pow2(n) 1<<n
// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void disk::read_block(blockid_t id, char *buf)
{
  /*
   *your lab1 code goes here.
   *if id is smaller than 0 or larger than BLOCK_NUM 
   *or buf is null, just return.
   *put the content of target block into buf.
   *hint: use memcpy
  */
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf)
{
  /*
   *your lab1 code goes here.
   *hint: just like read_block
  */
  if (id < 0 || id > BLOCK_NUM || buf == NULL)
    return;
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.

   *hint: use macro IBLOCK and BBLOCK.
          use bit operation.
          remind yourself of the layout of disk.
   */
   uint32_t bid = IBLOCK(INODE_NUM, BLOCK_NUM)+1;//first file block
   char buf[BLOCK_SIZE];
   for(; bid<BLOCK_NUM; bid++){//scan one by one
       d->read_block(BBLOCK(bid), buf);
       uint32_t bidchar = bid % BLOCK_SIZE;//the char that represent bid
       char *bits = &(buf[bidchar]);
       if((*bits & (pow2(bid%8))) == 0){//check free
           *bits |= (pow2(bid%8));
           d->write_block(BBLOCK(bid), buf);
           return bid;
       }
       
   }
   return -1;
}

void block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  char buf[BLOCK_SIZE];
  d->read_block(BBLOCK(id), buf);
  uint32_t bidchar = id % BLOCK_SIZE;//the char that represent bid
  char *bits = &(buf[bidchar]);
  *bits &= (~pow2(bidchar));
  d->write_block(BBLOCK(id), buf);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
}

void block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1)
  {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
    
   * if you get some heap memory, do not forget to free it.
   */
  //check size
  if (INODE_NUM <= 1)
    return 0;
  //varibles
  uint32_t inum;
  char buf[BLOCK_SIZE];
  inode_t *inode = NULL;
  //check inode
  for (inum = 1; inum < INODE_NUM; inum++)
  {
    bm->read_block(IBLOCK(inum, BLOCK_NUM), buf);
    inode = (inode_t *)buf + inum % IPB;
    if (inode->type == 0)
    {
      inode->type = type;
      inode->size = 0;
      inode->atime = time(NULL);
      inode->mtime = time(NULL);
      inode->ctime = time(NULL);
      bm->write_block(IBLOCK(inum, BLOCK_NUM), buf);
      return inum;
    }
  }
  return 0;
}
void inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   * do not forget to free memory if necessary.
   */
  inode_t *inode = get_inode(inum);
  // judge is a free inode block or not

  memset(inode, 0, sizeof(inode_t));
  put_inode(inum, inode);
  free(inode);
  return;
}

/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *
inode_manager::get_inode(uint32_t inum)
{
  struct inode *inode, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM)
  {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode *)buf + inum % IPB;
  if (ino_disk->type == 0)
  {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  inode = (struct inode *)malloc(sizeof(struct inode));
  *inode = *ino_disk;

  return inode;
}

void inode_manager::put_inode(uint32_t inum, struct inode *inode)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (inode == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode *)buf + inum % IPB;
  *ino_disk = *inode;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
  
  char hdblock[BLOCK_SIZE];
  char block[BLOCK_SIZE];
  char *buf;
  inode_t *inode = get_inode(inum);
  *size = inode->size;
  buf = (char *) malloc(inode->size);
  *buf_out = buf;
  uint32_t rblocks = *size / BLOCK_SIZE + 1; //how many blocks required
  if(*size%BLOCK_SIZE==0)rblocks-=1;
  uint32_t i;
  // direct blocks
  for (i = 0; i < NDIRECT && i < rblocks; i++){
    if (i != rblocks - 1)
    {
      bm->read_block(inode->blocks[i], block);
      memcpy((buf + i * BLOCK_SIZE),block,BLOCK_SIZE);
    }
    else
    {
      bm->read_block(inode->blocks[i], block);
      memcpy(buf + i * BLOCK_SIZE, block, *size - i * BLOCK_SIZE);
    }
  }
  if(rblocks>NDIRECT){
  // indirect block
  bm->read_block(inode->blocks[NDIRECT], hdblock);
  blockid_t *ndblock = (blockid_t *) hdblock;
  for (; i < rblocks; i++)
  {
    if (i != rblocks - 1)
    {
      bm->read_block(ndblock[i - NDIRECT], block);
      memcpy((buf + i * BLOCK_SIZE),block,BLOCK_SIZE);
    }
    else
    {
      bm->read_block(ndblock[i - NDIRECT], block);
      memcpy(buf + i * BLOCK_SIZE, block, *size - i * BLOCK_SIZE);
    }
  }
}
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode.
   * you should free some blocks if necessary.
   */
  char block[BLOCK_SIZE];
  char hdblock[BLOCK_SIZE];
  inode_t *inode = get_inode(inum);
  uint32_t oldsize = inode->size / BLOCK_SIZE + 1;
  uint32_t i = 0;
  if (inode->size % BLOCK_SIZE == 0)
    oldsize -= 1;
  uint32_t newsize = size / BLOCK_SIZE + 1;
  if (size % BLOCK_SIZE == 0)
    newsize -= 1; //buggy
  //need more
  if (oldsize < newsize)
  {
    if (newsize <= NDIRECT)
    {
      for (i = oldsize; i < newsize; i++)
      {
        inode->blocks[i] = bm->alloc_block();
      }
    }
    else
    {
      if (oldsize <= NDIRECT)//newsize>NDIRECT
      {
        for (i = oldsize; i < NDIRECT; i++)
        {
          inode->blocks[i] = bm->alloc_block();
        }
        inode->blocks[NDIRECT] = bm->alloc_block();

        for (i = NDIRECT; i < newsize; i++)
        {
          *((blockid_t *)hdblock + (i - NDIRECT)) = bm->alloc_block();
        }
        bm->write_block(inode->blocks[NDIRECT], hdblock);
      }
      else
      {
        bm->read_block(inode->blocks[NDIRECT], hdblock);
        for (i = oldsize; i < newsize; i++)
        {
          *((blockid_t *)hdblock + (i - NDIRECT)) = bm->alloc_block();
        }
        bm->write_block(inode->blocks[NDIRECT], hdblock);
      }
    }
  }
  //need free
  else if(newsize<oldsize)
  {
    if (newsize > NDIRECT)
    {
      bm->read_block(inode->blocks[NDIRECT], hdblock);
      for (i = oldsize; i > newsize; i--)
      {
        bm->free_block(*((blockid_t *)hdblock + (i - NDIRECT)));
      }
    }
    else if (oldsize > NDIRECT && newsize <= NDIRECT)
    {
      bm->read_block(inode->blocks[NDIRECT], hdblock);
      for (i = oldsize; i > NDIRECT; i--)
      {
        bm->free_block(*((blockid_t *)hdblock + (i - NDIRECT)));
      }
      for (i = NDIRECT; i > newsize; i--)
      {
        bm->free_block(inode->blocks[i]);
      }
    }
    else
    { //all <=NDIRECT
      for (i = oldsize; i > newsize; i--)
      {
        bm->free_block(inode->blocks[i]);
      }
    }
  }

  //write

  for (i = 0; i < NDIRECT && i < newsize; i++)
  {
    if (size - i * BLOCK_SIZE > BLOCK_SIZE)
    {
      bm->write_block(inode->blocks[i], buf + i*BLOCK_SIZE );
    }
    else
    {
      int left = size - i * BLOCK_SIZE;
      memcpy(block, buf + i * BLOCK_SIZE, left);
      bm->write_block(inode->blocks[i], block);
    }
  }
  //write ndirect
  if(newsize>NDIRECT){
  bm->read_block(inode->blocks[NDIRECT], hdblock);
  for (; i < newsize; i++)
  {
    blockid_t ndblock = *((blockid_t *)hdblock + i - NDIRECT);
    if (size - i * BLOCK_SIZE > BLOCK_SIZE)
    {
      bm->write_block(ndblock, buf + i * BLOCK_SIZE);
    }
    else
    {
      int left = size - i * BLOCK_SIZE;
      memcpy(block, buf + i * BLOCK_SIZE, left);
      bm->write_block(ndblock, block);
    }
  }
}

  //update
  inode->size = size;
  inode->mtime = time(NULL);
  inode->ctime = time(NULL);
  put_inode(inum, inode);
  free(inode);
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  inode_t *inode = get_inode(inum);

  if (inode == NULL)
  {
    a.type = 0;
    a.size = 0;
    a.atime = 0;
    a.mtime = 0;
    a.ctime = 0;
  }
  else
  {
    a.type = inode->type;
    a.size = inode->size;
    a.atime = inode->atime;
    a.mtime = inode->mtime;
    a.ctime = inode->ctime;
  }

  free(inode);
}

void inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   * do not forget to free memory if necessary.
   */
   inode_t * inode = get_inode(inum);
   uint32_t size = inode->size/ BLOCK_SIZE+1;
   uint32_t i=0;
   if (inode->size % BLOCK_SIZE == 0)size -= 1; //buggy
   for (i = 0; i < size; i++) {
    bm->free_block(inode->blocks[i]);
  }
   if(size>NDIRECT){
     char hdblock[BLOCK_SIZE];
     bm->read_block(inode->blocks[NDIRECT], hdblock);
     for (; i < size; i++) {
       bm->free_block(*((blockid_t *)hdblock + i-NDIRECT));
     }
     bm->free_block(inode->blocks[NDIRECT]);
   }
   free_inode(inum);
   free(inode);
}

// CPSC 341 - HW3:  File System Disk Interface
// This implements a simulated disk consisting of an array of blocks.

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <cstdlib>
using namespace std;

#include "disk.h"

bool mount_disk(const char *file_name, int *fd)
{
  *fd = open(file_name, O_RDWR);
  if (*fd != -1) return false;

  *fd = open(file_name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if (*fd == -1) {
    cerr << "Could not create disk" << endl;
    exit(-1);
  }

  return true;
}

void unmount_disk(int fd)
{
  close(fd);
}
  
void read_disk_block(int fd, int block_num, void *block)
{
  off_t offset;
  off_t new_offset;
  ssize_t size; 

  if (block_num < 0 || block_num >= NUM_BLOCKS) {
    cerr << "Invalid block size" << endl;
    exit(-1);
  }

  offset = block_num * BLOCK_SIZE;
  new_offset = lseek(fd, offset, SEEK_SET);
  if (offset != new_offset) {
    cerr << "Seek failure" << endl;
    exit(-1);
  }

  size = read(fd, block, BLOCK_SIZE);
  if (size != BLOCK_SIZE) {
    cerr << "Failed to read entire block" << endl;
    exit(-1);
  }
}

void write_disk_block(int fd, int block_num, void *block)
{
  off_t offset;
  off_t new_offset;
  ssize_t size; 

  if (block_num < 0 || block_num >= NUM_BLOCKS) {
    cerr << "Invalid block size" << endl;
    exit(-1);
  }

  offset = block_num * BLOCK_SIZE;
  new_offset = lseek(fd, offset, SEEK_SET);
  if (offset != new_offset) {
    cerr << "Seek failure" << endl;
    exit(-1);
  }

  size = write(fd, block, BLOCK_SIZE);
  if (size != BLOCK_SIZE) {
    cerr << "Failed to write entire block" << endl;
    exit(-1);
  }
}

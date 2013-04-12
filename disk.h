// CPSC 341 - HW3:  File System Disk Interface

// This implements a simulated disk consisting of an array of blocks.

#ifndef DISK_H
#define DISK_H

const int BLOCK_SIZE = 128;	    	 // must be an even power of two
const int NUM_BLOCKS = (BLOCK_SIZE * 8); // set so a bitmap can fit in one block

// Opens the file "file_name" that represents the disk.  If the file does
// not exist,  file is created.  The descriptor is returned in output
// parameter fd.  Returns true if a file is created and false if the file
// exists.  Any error aborts the program.
bool mount_disk(const char *filename, int *fd);

// Closes the file descriptor that represents the disk.
void unmount_disk(int fd);
  
// Reads disk block block_num from the disk pointed to by fd into the data
// structure pointed to by block.
void read_disk_block(int fd, int block_num, void *block);

// Writes the data in block to disk block block_num pointed to by fd.
void write_disk_block(int fd, int block_num, void *block);

#endif

// This program implements a simple file system.  A shell is
// provided to enter assorted file system commands.

//Name: Kenneth Ordona
//Class; CPSC 341
//This program simulates a unix file system that has the basic uni file system
//commands implemented. These commands include cd, rmdir, rm, create, mkdir, space,
//ls, append, and cat. These functions outline the basic program functions
// that allow for a unix file system. 



#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream> 
#include <string>
#include <cmath>
using namespace std;

#include "disk.h"

struct cmd_t
{
	char *cmd_name;	// name of command
	char *file_name;	// name of file
	char *data;		// data (append only)
};
bool make_cmd(char *cmd_str, struct cmd_t &command);

// Constants

const char *PROMPT_STRING = "hw3> ";
const char *DISK_NAME = "DISK";
const unsigned int DIR_MAGIC_NUM = 0xFFFFFFFF;
const unsigned int INODE_MAGIC_NUM = 0xFFFFFFFE;
const int MAX_CMD_LINE = 500;
const int MAX_FNAME_SIZE = 6;
const int MAX_BLOCKS = ((BLOCK_SIZE - 8) / 2);
const int MAX_FILES = (BLOCK_SIZE / 8 - 1);
const int MAX_FILE_SIZE	= (MAX_BLOCKS * BLOCK_SIZE);
const int ROOT_DIR = 1;
const int FILE_BLOCK = 1;
const int BYTE_SIZE = 8;

// Block types

struct superblock_t {
	unsigned char bitmap[BLOCK_SIZE]; // bitmap of free blocks
};

struct dirblock_t {
	unsigned int magic;		// magic number, must be DIR_MAGIC_NUM
	unsigned int num_entries;	// number of files in directory
	struct {
		char name[MAX_FNAME_SIZE];	// file name
		short block_num;		// block number of file (0 - unused)
	} dir_entries[MAX_FILES];	// list of directory entries
};

struct inode_t {
	unsigned int magic;		// magic number, must be INODE_MAGIC_NUM
	unsigned int size;		// file size in bytes
	short blocks[MAX_BLOCKS];	// array of direct indices to data blocks
};

struct datablock_t {
	char data[BLOCK_SIZE];	// data (BLOCK_SIZE bytes)
};


// Command processing
//this function intializes a new directory
dirblock_t mkdir();					
//this function takes a new initalized directory and places it in the current subdir
void makeDir(dirblock_t curBlock, cmd_t command, int curDir, int disk);	
//this function outputs the current subdirectory
void ls(dirblock_t curBlock, cmd_t command, int disk);
//this function returns whether or not the blockNum is a dir or not
bool isDir(short blockNum, int disk);
//this function pushes the user into an existing subdirectory
void cd(dirblock_t curBlock, cmd_t command, short &curDir, int disk);
//this function removes a current existing directory(or not)
void rmDir(dirblock_t curBlock, cmd_t command, short curDir, int disk);
//this function deletes all the associated datablocks with the file
dirblock_t deleteAll(short blockNum, int disk);
//this function creates a file with iNode implementation
void createF(dirblock_t curBlock, cmd_t command, short curDir, int disk);
//this function appends to the current file
void append(dirblock_t curBlock, cmd_t command, int disk);
//this function outputs the iNode 
void cat(dirblock_t curBlock, cmd_t command, int disk);
//this function removes the iNode file given
void rm(dirblock_t curBlock, cmd_t command, short curDir, int disk);
//this function outputs the current space of the disk
void space(int disk);
//this function outputs the current taken blocks of the disk
int getTaken(int disk);
//this function initializes a iNode & returns it
inode_t create();

//core disk functions

// Opens the simulated disk file. If a disk file is created, this
// routines also "formats" the disk by initializing special blocks
// 0 (superblock) and 1 (root directory). */
int open_disk()
{
	int fd;		// file descriptor for disk
	bool new_disk;	// set if new disk was created
	int i;		// loop traversal variable

	struct superblock_t super_block;	// used to initialize block 0
	struct dirblock_t dir_block;		// used to initialize block 1
	struct datablock_t data_block;	// used to initialize other blocks

	// Mount the disk
	new_disk = mount_disk(DISK_NAME, &fd);

	// Check for a new disk.  If we have a new disk, we must continue and
	// format the disk.
	if (!new_disk) return fd;

	// Initialize the superblock
	super_block.bitmap[0] = 0x3;		// mark blocks 0 and 1 as used
	for (i = 1; i < BLOCK_SIZE; i++) {
		super_block.bitmap[i] = 0;
	}

	// Write the superblock to block 0
	write_disk_block(fd, 0, (void *) &super_block);

	// Initialize the root directory
	dir_block.magic = DIR_MAGIC_NUM;
	dir_block.num_entries = 0;
	for (i = 0; i < MAX_FILES; i++) {
		dir_block.dir_entries[i].block_num = 0;
	}

	// Write the root directory to block 1
	write_disk_block(fd, 1, (void *) &dir_block);

	// Initialize the data block to all zeroes
	for (i = 0; i < BLOCK_SIZE; i++) {
		data_block.data[i] = 0;
	}

	// Write the data block to all other blocks on disk
	for (i = 2; i < NUM_BLOCKS; i++) {
		write_disk_block(fd, i, (void *) &data_block);
	}

	return fd;
}

// Gets a free block from the disk.
short get_free_block(int disk)
{
	struct superblock_t super_block;	// super block - block 0
	int byte;	// byte of super block
	int bit;	// bit checked to see if block is available

	// get superblock
	read_disk_block(disk, 0, (void *) &super_block);

	// look for first available block
	for (byte = 0; byte < BLOCK_SIZE; byte++) {

		// check to see if byte has available slot
		if (super_block.bitmap[byte] != 0xFF) {

			// loop to check each bit
			for (bit = 0; bit < 8; bit++) {
				int mask = 1 << bit;	// used to determine if bit is set or not
				if (mask & ~super_block.bitmap[byte]) {
					// Available block is found: set bit in bitmap, write result back
					// to superblock, and return block number.
					super_block.bitmap[byte] |= mask;
					write_disk_block(disk, 0, (void *) &super_block);
					return (byte * 8) + bit;
				}
			}
		}
	}

	// disk is full
	return 0;
}

// Reclaims block making it available for future use.
void reclaim_block(int disk, short block_num)
{
	struct superblock_t super_block;	// super block - block 0
	int byte = block_num / 8;		// byte number
	int bit = block_num % 8;		// bit number
	unsigned char mask = ~(1 << bit);	// mask to clear bit

	// get superblock
	read_disk_block(disk, 0, (void *) &super_block);

	// clear bit
	super_block.bitmap[byte] &= mask;

	// write back superblock
	write_disk_block(disk, 0, (void *) &super_block);
}

int main()
{
	int disk;			  // file descriptor for disk
	char cmd_str[MAX_CMD_LINE + 1]; // command line
	struct cmd_t command;		  // command struct
	bool status;			  // check for invalid command line
	dirblock_t newBlock;	
	dirblock_t curBlock;

	inode_t tempFile;

	short newBlockNum;
	short curDir = ROOT_DIR;		//set to root directory initially
	// Uncomment this section to make sure the size of the blocks are
	// equal to the block size.
#if 0
	cout << "superblock size: " << sizeof(struct superblock_t) << endl;
	cout << "dirblock size: " << sizeof(struct dirblock_t) << endl;
	cout << "inode size: " <<  sizeof(struct inode_t) << endl;
	cout << "datablock size: " << sizeof(struct datablock_t) << endl;
#endif

	// Open the disk
	disk = open_disk();

	while (1) {

		// Print prompt and get command line
		cout << PROMPT_STRING;
		if (fgets(cmd_str, MAX_CMD_LINE, stdin) == NULL) continue;

		// Create the command structure, checking for invalid command lines
		status = make_cmd(cmd_str, command);
		if (!status) continue;

		read_disk_block(disk, curDir, (void *) &curBlock);
		// Look for the matching command
		if (strcmp(command.cmd_name, "mkdir") == 0) 
			makeDir(curBlock, command, curDir, disk);

		else if (strcmp(command.cmd_name, "ls") == 0) 
			ls(curBlock, command, disk);
		
		else if (strcmp(command.cmd_name, "cd") == 0) 
			cd(curBlock, command, curDir, disk);
		
		else if (strcmp(command.cmd_name, "home") == 0) {
			curDir = ROOT_DIR;
			cout << "Home directory entered. " << endl;
		}
		else if (strcmp(command.cmd_name, "rmdir") == 0) 
			rmDir(curBlock, command, curDir, disk);
		
		else if (strcmp(command.cmd_name, "create") == 0) 
			createF(curBlock, command, curDir, disk);

		else if (strcmp(command.cmd_name, "append") == 0) 
			append(curBlock, command, disk);

		else if (strcmp(command.cmd_name, "cat") == 0) 
			cat(curBlock, command, disk);

		else if (strcmp(command.cmd_name, "rm") == 0) 
			rm(curBlock, command, curDir, disk);
		
		else if (strcmp(command.cmd_name, "space") == 0) 
			space(disk);

		else if (strcmp(command.cmd_name, "quit") == 0) {
			unmount_disk(disk);
			exit(0);
		}
		else {
			cerr << "ERROR: Invalid command not detected by make_cmd" << endl;
		}
	}

	return 0;
}

//returns whether a block is a directory
bool isDir(short blockNum, int disk)
{
	dirblock_t tempBlock;
	read_disk_block(disk, blockNum, (void*) &tempBlock);

	if(tempBlock.magic == DIR_MAGIC_NUM)
		return true;
	else
		return false;
}

//intializes a directory and returns it
dirblock_t mkdir(){
	dirblock_t tempDir;
	tempDir.magic = DIR_MAGIC_NUM;
	tempDir.num_entries = 0;

	return tempDir;
}

//this function initializes a iNode
inode_t create(){
	inode_t tempINode;
	tempINode.magic = INODE_MAGIC_NUM;
	tempINode.size = 0;
	for(int i = 0; i < MAX_BLOCKS; i++)
		tempINode.blocks[i] = 0;

	return tempINode;
}

//this function creates a directory
void makeDir(dirblock_t curBlock, cmd_t command, int curDir, int disk){
	bool there = false;
	bool isSpace = false;
	short newBlockNum;
	dirblock_t newBlock;


	for(int i = 0; i < MAX_FILES; i++)
	{
		if(strcmp(command.file_name, curBlock.dir_entries[i].name) == 0 && isDir(curBlock.dir_entries[i].block_num, disk))
			there = true;
		if(curBlock.dir_entries[i].block_num == 0)
			isSpace = true;
	}

	if(getTaken(disk) >= 1024)
		isSpace = false;

	if(!there && isSpace){
		newBlock = mkdir();
		newBlockNum = get_free_block(disk);
		write_disk_block(disk, newBlockNum, (void *) &newBlock);

		strcpy(curBlock.dir_entries[curBlock.num_entries].name, command.file_name);
		curBlock.dir_entries[curBlock.num_entries].block_num = newBlockNum;
		curBlock.num_entries++;
		write_disk_block(disk, curDir, (void *) &curBlock);
		cout << "Directory " << command.file_name << " is created." << endl; 
	}
	else if(!there && !isSpace)
		cout << "There is no space for the directory to be created in the current directory. " << endl;
	else
		cout << "Directory " << command.file_name << " is already created." << endl; 
}

//this function outputs the current subdirs and subfiles
void ls(dirblock_t curBlock, cmd_t command, int disk){
	cout << "Name  Block   Type   Bytes  NumBlocks(Full Blocks)" << endl;
	inode_t tempBlock1;
	int numBlocks;
	int count = 0;
	for(int i = 0; i < MAX_FILES; i++)
	{
		count++;
		if(isDir(curBlock.dir_entries[i].block_num, disk) && curBlock.dir_entries[i].block_num != 0)
		{
			cout << curBlock.dir_entries[i].name << "     " << curBlock.dir_entries[i].block_num 
				<< "      " << "dir" << endl;
		}
		else if(curBlock.dir_entries[i].block_num != 0)
		{
			read_disk_block(disk, curBlock.dir_entries[i].block_num, (void *) &tempBlock1);
			numBlocks = ((tempBlock1.size/BLOCK_SIZE))+FILE_BLOCK;
			cout << curBlock.dir_entries[i].name << "     " << curBlock.dir_entries[i].block_num 
				<< "      " << "file" <<"      "<< tempBlock1.size << "      " << numBlocks << endl;
		}
	}
	cout << endl;
}

//this functions turns the current directory into the parameter that is passed
void cd(dirblock_t curBlock, cmd_t command, short&curDir, int disk){
	bool found = false;
	bool dir = false;
	for(int i = 0; i < MAX_FILES; i++)
	{
		if(strcmp(command.file_name, curBlock.dir_entries[i].name) == 0)
		{
			found = true;
			if(isDir(curBlock.dir_entries[i].block_num, disk)){
				curDir = curBlock.dir_entries[i].block_num;
				dir = true;
			}
		}
	}
	if(found && dir)
		cout << "Directory " << command.file_name << " entered." << endl;
	else if(found && !dir)
		cout << "This is not a directory. Cannot enter." << endl;
	else
		cout << "Directory not found. " << endl;
}

//this function removes a directory
void rmDir(dirblock_t curBlock, cmd_t command, short curDir, int disk){
	char name[MAX_FNAME_SIZE] = "";
	bool found = false;
	bool dir = false;
	bool empty = false;
	dirblock_t tempBlock;
	for(int i = 0; i < MAX_FILES; i++)
	{
		if(strcmp(command.file_name, curBlock.dir_entries[i].name) == 0)
		{	found = true;
			if(isDir(curBlock.dir_entries[i].block_num, disk)){
				dir = true;
				read_disk_block(disk, curBlock.dir_entries[i].block_num, (void*) &tempBlock);
				if(tempBlock.num_entries == 0){
					reclaim_block(disk, curBlock.dir_entries[i].block_num);
					empty = true;
					curBlock.num_entries--;
					curBlock.dir_entries[i].block_num = 0;
					strcpy(curBlock.dir_entries[i].name, name);
					write_disk_block(disk, curDir, (void *) &curBlock);
				}
			}
		}
	}
	if(found && empty)
		cout << "Directory " << command.file_name << " deleted." << endl;
	else if(found && !dir)
		cout << "This file is not a directory. Please use rm." << endl;
	else if(found && !empty)
		cout << "Directory " << command.file_name << " is not empty. Cannot delete." << endl;
	else
		cout << "Directory not found. " << endl;
}

//this function will create a new iNode file
void createF(dirblock_t curBlock, cmd_t command, short curDir, int disk){
	inode_t newFile;
	bool there = false;
	bool isSpace = false;
	int newBlockNum;
	int emptyIndex;
	char empty[BLOCK_SIZE] = "";
	datablock_t tempDa;
	int tempAdd;
	for(int i = 0; i < MAX_FILES; i++)
	{
		if(strcmp(command.file_name, curBlock.dir_entries[i].name) == 0 && !isDir(curBlock.dir_entries[i].block_num, disk))
			there = true;
		if(curBlock.dir_entries[i].block_num == 0){
			emptyIndex = i;
			isSpace = true;
		}
	}

	if(getTaken(disk) >= 1024)
		isSpace = false;

	if(!there && isSpace){
		newFile = create();
		newBlockNum = get_free_block(disk);

		strcpy(curBlock.dir_entries[emptyIndex].name, command.file_name);
		curBlock.dir_entries[emptyIndex].block_num = newBlockNum;
		curBlock.num_entries++;
		tempAdd = newBlockNum;
		for(int j = 0; j < MAX_BLOCKS; j++)
		{
			newFile.blocks[j] = 0;
		}
		newBlockNum = get_free_block(disk);
		newFile.blocks[0] = newBlockNum;
		strcpy(tempDa.data, empty);

		write_disk_block(disk, curDir, (void *) &curBlock);
		write_disk_block(disk, tempAdd, (void *) &newFile);
		write_disk_block(disk, newBlockNum, (void*)&tempDa);

		cout << "File " << command.file_name << " is now created. " << endl;
	}
	else if(!there && !isSpace)
		cout << "There is no space for the file to be created in the current directory. " << endl;
	else
		cout << "File " << command.file_name << " is already created. " << endl;
}

//this functionw ill output to the current iNode that is passed in
void append(dirblock_t curBlock, cmd_t command, int disk){
	datablock_t tempData;
	int sizeStr = (int)strlen(command.data);
	int curIndex = 0;
	int pastIndex = 0;
	int fileInd = 0;
	inode_t tempFile;
	int newBlockNum;
	bool start = true;
	bool space = true;
	bool file = false;
	bool found = false;

	//char *temp;
	//
	//temp = strchr(command.data, '\n');
	//if(temp!= NULL) 
	//	*temp = 0;

	for(int i = 0; i < MAX_FILES; i++)
	{
		if(strcmp(command.file_name, curBlock.dir_entries[i].name) == 0)
		{
			found = true;
			if(!isDir(curBlock.dir_entries[i].block_num, disk)){
				file = true;
				read_disk_block(disk, curBlock.dir_entries[i].block_num, (void*)&tempFile);
				
				if((ceil(tempFile.size+sizeStr)) > MAX_FILE_SIZE)
					space = false;
				
				for(int k = 0; k < MAX_BLOCKS; k++){
					if(tempFile.blocks[k] != 0)
					{
						curIndex = k;
						pastIndex = curIndex;
					}
				}
				read_disk_block(disk, tempFile.blocks[curIndex], (void*) &tempData);
					if(space){
						for(int j = 0; j < sizeStr; j++)
						{
							
							if(curIndex != pastIndex)
							{
								read_disk_block(disk, tempFile.blocks[curIndex], (void*) &tempData);
								pastIndex = curIndex;
							}

							fileInd = (tempFile.size%128);

							if(tempFile.size % 128 == 0 && !start && tempFile.size != MAX_FILE_SIZE)
							{
								newBlockNum = get_free_block(disk);
								curIndex++;
								read_disk_block(disk, tempFile.blocks[curIndex], (void*)&tempData);
								tempData.data[0] = command.data[j];
								tempFile.size++;
								write_disk_block(disk, newBlockNum, (void*) &tempData);
								tempFile.blocks[curIndex] = newBlockNum;
							}
							else{
								tempData.data[fileInd] = command.data[j];
								tempFile.size++;
								write_disk_block(disk, tempFile.blocks[curIndex], (void*)&tempData);
							}
							start = false;

						}
						
					}
					write_disk_block(disk, curBlock.dir_entries[i].block_num, (void*)&tempFile);
					
			}
		}

	}
	if(!found)
		cout << "File was not found!" << endl;
	else if(found && !file)
		cout << "This is a directory. Cannot output contents. " << endl;
	else if(file && !space)
		cout << "No more free space available in this file! " << endl;

}

//this function outputs the given iNode block
void cat(dirblock_t curBlock, cmd_t command, int disk){
	inode_t tempFile;
	
	bool found = false;
	bool file = false;
	for(int i = 0; i < MAX_FILES; i++)
	{
		if(strcmp(command.file_name, curBlock.dir_entries[i].name) == 0)
		{
			found = true;
			if(!isDir(curBlock.dir_entries[i].block_num, disk)){
				file = true;
				read_disk_block(disk, curBlock.dir_entries[i].block_num, (void*)&tempFile);
				cout << "The file " << command.file_name <<" holds: ";
				for(int j = 0; j < MAX_BLOCKS; j++)
				{
					if(tempFile.blocks[j] > 0)
					{
						datablock_t tempD;
						read_disk_block(disk, tempFile.blocks[j], (void*)&tempD);
						int dataSize = strlen(tempD.data);
						if(dataSize > BLOCK_SIZE)
							for(int k = 0; k < BLOCK_SIZE; k++)
								cout << tempD.data[k];
						else
							for(int k = 0; k < strlen(tempD.data); k++)
								cout << tempD.data[k];
					}
				}
			}
		}
	}
	if(found && !file)
		cout << "This is not a file! Cannot output contents.";
	else if(!found)
		cout << "File does not exist.";

	cout << endl;
}

//this function removes the passed in block
void rm(dirblock_t curBlock, cmd_t command, short curDir, int disk){
	char name[MAX_FNAME_SIZE] = "";
	char empty[BLOCK_SIZE] = "";
	bool found = false;
	inode_t tempFile;
	inode_t tempFile2;
	datablock_t dataTemp;
	for(int i = 0; i < MAX_FILES; i++)
	{
		if(strcmp(command.file_name, curBlock.dir_entries[i].name) == 0)
		{
			if(!isDir(curBlock.dir_entries[i].block_num, disk)){
				read_disk_block(disk, curBlock.dir_entries[i].block_num, (void*) &tempFile);
				for(int j = 0; j < ((tempFile.size/BLOCK_SIZE)+FILE_BLOCK); j++)
				{
					if(tempFile.blocks[j] != 0){
						read_disk_block(disk, tempFile.blocks[j], (void*)&dataTemp);
						strcpy(dataTemp.data, empty);
						write_disk_block(disk, tempFile.blocks[j], (void*)&dataTemp);
						cout << tempFile.blocks[j] << endl;
						reclaim_block(disk, tempFile.blocks[j]);
						tempFile.blocks[j] = 0; 
					}

				}
				reclaim_block(disk, curBlock.dir_entries[i].block_num);

				found = true;
				curBlock.num_entries--;
				curBlock.dir_entries[i].block_num = 0;
				strcpy(curBlock.dir_entries[i].name, name);
				write_disk_block(disk, curDir, (void *) &curBlock);
			}
		}
	}
	if(found)
		cout << "File " << command.file_name << " deleted." << endl;
	else
		cout << "File not found. " << endl;	
}

//this function returns the space left in the disk
void space(int disk)
{
	int totalSpace;
	int available =0;
	int taken = 0;
	int byte; 
	int bit;
	superblock_t supBlock;

	read_disk_block(disk, 0, (void*)&supBlock);

	for (byte = 0; byte < BLOCK_SIZE; byte++) {
		if (supBlock.bitmap[byte] != 0xFF) {

			// loop to check each bit
			for (bit = 0; bit < BYTE_SIZE; bit++) {
				int mask = 1 << bit;	// used to determine if bit is set or not
				if (mask & ~supBlock.bitmap[byte])
					available++;
				else
					taken++;
			}
		}
		else
			taken += BYTE_SIZE;
	}
	totalSpace = (available+taken);
	cout << "Available blocks: " << available  << endl;
	cout << "Taken blocks: " << taken << endl;
	cout << "Total blocks: " << totalSpace << endl;
}

//this function returns the amount of space taken in the disk
int getTaken(int disk)
{
	int totalSpace;
	int available = 0;
	int taken = 0;
	int byte; 
	int bit;
	superblock_t supBlock;

	read_disk_block(disk, 0, (void*)&supBlock);

	for (byte = 0; byte < BLOCK_SIZE; byte++) {
		if (supBlock.bitmap[byte] != 0xFF) {

			// loop to check each bit
			for (bit = 0; bit < BYTE_SIZE; bit++) {
				int mask = 1 << bit;	// used to determine if bit is set or not
				if (!(mask & ~supBlock.bitmap[byte]))
					taken++;
			}
		}
		else
			taken += BYTE_SIZE;
	}
	return taken;
}

bool make_cmd(char *cmd_str, struct cmd_t &command)
{
	const char *DELIM  = " \t\n"; // delimiters
	int numtokens = 0;		// number of tokens
	const char *snew;		// command string without leading spaces
	char *temp_str;		// temporary string used by strtok

	// Remove spaces at beginning
	snew = cmd_str + strspn(cmd_str, DELIM);
	if (strlen(snew) == 0) {
		cerr << "Empty command line" << endl;
		return false;
	}

	// Create new string
	temp_str = (char *) new char[strlen(snew) + 1];
	if (temp_str == NULL) {
		cerr << "new failed" << endl; 
		exit(-1);
	}
	strcpy(temp_str, snew);

	// Count the number of tokens in the command */
	if (strtok(temp_str, DELIM) != NULL)
		for (numtokens = 1; strtok(NULL, DELIM) != NULL; numtokens++) ; 

	// Check for empty command line
	if (numtokens == 0) {
		cerr << "Empty command line" << endl;
		delete [] temp_str;
		return false;
	}

	// Extract the data
	strcpy(temp_str, snew);
	command.cmd_name = strtok(temp_str, DELIM);
	if (numtokens > 1) command.file_name = strtok(NULL, DELIM);
	if (numtokens > 2) command.data = strtok(NULL, DELIM);

	// Check for invalid command lines
	if (strcmp(command.cmd_name, "ls") == 0 ||
		strcmp(command.cmd_name, "home") == 0 ||
		strcmp(command.cmd_name, "space") == 0 ||
		strcmp(command.cmd_name, "quit") == 0)
	{
		if (numtokens != 1) {
			cerr << "Invalid command line: " << command.cmd_name;
			cerr << " has improper number of arguments" << endl;
			return false;
		}
	}
	else if (strcmp(command.cmd_name, "mkdir") == 0 ||
		strcmp(command.cmd_name, "cd") == 0 ||
		strcmp(command.cmd_name, "rmdir") == 0 ||
		strcmp(command.cmd_name, "create") == 0||
		strcmp(command.cmd_name, "cat") == 0 ||
		strcmp(command.cmd_name, "rm") == 0)
	{
		if (numtokens != 2) {
			cerr << "Invalid command line: " << command.cmd_name;
			cerr << " has improper number of arguments" << endl;
			return false;
		}
		if (strlen(command.file_name) >= MAX_FNAME_SIZE) {
			cerr << "Invalid command line: " << command.file_name;
			cerr << " is too long for a file name" << endl;
			return false;	
		}
	}
	else if (strcmp(command.cmd_name, "append") == 0)
	{
		if (numtokens != 3) {
			cerr << "Invalid command line: " << command.cmd_name;
			cerr << " has improper number of arguments" << endl;
			return false;
		}
		if (strlen(command.file_name) >= MAX_FNAME_SIZE) {
			cerr << "Invalid command line: " << command.file_name;
			cerr << " is too long for a file name" << endl;
			return false;
		}
	}
	else {
		cerr << "Invalid command line: " << command.cmd_name;
		cerr << " is not a command" << endl; 
		return false;
	} 

	return true;
}     


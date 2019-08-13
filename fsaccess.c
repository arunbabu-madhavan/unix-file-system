/**
 * 
 * Authors: Arun Babu Madhavan (axm170039), Mrugapphan Kannan (mxk170014), Srikumar Ramaswamy (sxr170016)
 * Purpose: UNIX v6 File system implementation
 * Usage: 
 *    The program will read a series of commands from the user and execute them.
 * 			(a) initfs will initialize the file system.
 *					initfs should accept three arguments:
 *						(1) the name of the (special) file that physically represents the disk,
 *						(2) number n1 indicating the total number of blocks in the disk (fsize) and
 *						(3) number n2 representing the total number of i-nodes in the disk.
 *			(b) q  Quit the program by saving all the work
 *			
 *			(c) cpin will create a new file  in the v6 file system and fill the contents of the newly created file with the contents of the externalfile.
 *					cpin will accept two arguments:
 *						(1) then filepath of the external file
 *						(2) the filepath of the v6 file
 *			(d) cpout will create an external and make the external file's content equal to v6 file		
 *					cpout will accept two arguments:
 *						(1) the filepath of the v6 file
 *						(2) the filepath of the external file
 *			(e) mkdir	will create a new directory in the v6 file system
 *					mkdir will accept 1 argument
 *						(1) the file path of the directory
 *			(f) rm will remove the file/directory from the v6 file system
 *					rm will accept 1 argument
 *						(1)	the filepath of the v6 file
 *  How to run:
 *    Compile using:
 *        cc fsaccess.c -lm -o fsaccess 
 *    Inputs:   
 * 		Create a file using touch command in unix
 * 			Eg: touch test.data
 *		  Supported commands:
 *				       initfs test.data 200 20
 *					   cpin e.txt e
 *					   cout e e1.txt
 *					   mkdir folder1
 *					   rm e
 *					   q
 *    Execute using:
 * 	      ./fsaccess
 * 				When prompted by the program give any one of the
 * 														 supported commands to test
**/

#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<math.h>
#include<time.h>



/* Toogle status to print/ not print execution steps in the console 1 - print , 0 - don't print*/
#define DEBUG 0

#define BLOCK_SIZE 1024
#define INODE_SIZE_BYTES 64

// MACRO function to print if DEBUG is set
#if DEBUG
    #define DEBUG_LOG printf
#else
    #define DEBUG_LOG DONT_PRINT
#endif
// Empty MACRO function to not print if DEBUG is not set
#define DONT_PRINT(x,args...) {}

#define NUMBER_OF_BLOCKS_PER_INDIRECTION (int) (BLOCK_SIZE/sizeof(int))

// Gives the length of the array
#define len(X)  (int)(sizeof(X)/sizeof(*(X)))

#define NUMBER_OF_INODES_PER_BLOCK  BLOCK_SIZE/INODE_SIZE_BYTES

#define BLOCK_POSITION(n) (n) * BLOCK_SIZE

// block 0 is left free, block 1 stores the super block, so i nodes start from block 2
#define INODE_POSITION(n) ((2) * BLOCK_SIZE) + ((n-1) * INODE_SIZE_BYTES) // inode number starts with 1



// Super Block Struct
struct superblock_t {
	unsigned int isize; /* 4 bytes*/
	unsigned int fsize; /* 4 bytes*/
	unsigned int nfree; /* 4 bytes*/
	unsigned int free[150]; /* 600 bytes */
    unsigned int ninode; /* 4 bytes */
    unsigned int inode[100]; /* 400 bytes */
    char flock; /* 1 byte */
    char ilock; /* 1 byte */
    char fmod;  /* 1 byte */
    unsigned short time[2]; /* 4 bytes */
}; // 1023 bytes 

// inode struct
typedef struct  {
	unsigned short flags; /* 2 byte */
	char nlinks; /* 1 byte */
	char uid; /* 1 byte */
	char gid; /* 1 byte */
	unsigned short size0; /* 2 bytes */ //To support a file size of 4 GB
	unsigned short size1; /* 2 bytes */ 
	/* It will use triple level chaining at addr[10] for large files 
			and single chaining for small files. so a 256 * 256 * 256 * 1024 = 16 GB  */
	unsigned int addr[11]; /* 44 bytes */
	unsigned short acttime[2]; /* 4 bytes */
	unsigned short modtime[2]; /* 4 bytes */
} inode_t; // 61 bytes

//  Free block struct
typedef struct {
	unsigned int nfree;
	unsigned int free[150];
} firstfreeblock_t; 

// Directory item 
#pragma pack(1) // exact fitting no extra padding
typedef struct{
	unsigned int inode; /* 4 bytes */
	char name[28];   /* 14 bytes */
} directoryitem_t; // 32 bytes


// Single Indirection block
#pragma pack(1) // exact fitting no extra padding
typedef struct {
	unsigned int blockNumbers[BLOCK_SIZE/sizeof(int)];
} singleIndirectblock_t; //1024 bytes

//Directory content
typedef struct{
	unsigned int inode; 
	char name[28];   
	unsigned short isDirectory;
	unsigned int fileSize;
} directoryContent;

/* Globals Constants */
char delimiter[] = " ";


/* Function declarations */
int processCommand(char cmd[100]);
void initfs(char *args);
void add_to_free_list(int blockNumber);
void create_new_directory(int datablockNumber,int parentinode,int newinode);
unsigned int get_free_block();
unsigned int get_free_inode();
void print_free_inode_list();
void print_free_block_list();
void make_dir(char *args);
void load(char *args);
void makeLargefile(int inode_number);
void add_directoryEntry_to_parentDir(char *name,int parentinode,int newinode);
void rm(char *args);
void deleteDir(int inode_number);
void deleteFile(int inode_number);
void deleteDirectoryEntry(int parent_inode_number,int inode_number);
directoryContent* getDirectoryContents(int* noOfitems,int inode_number);
int fileExists(char *fileName, int parent_inode_number);
void cpin(char *args);
void cpout(char *args);
int searchFileInFileSystem(char *path, int *parent_inode_number,char *fileName);
void listDir();
void changeParentDir(char *args);
void addDataBlockToInode(int inode_number,int blockNumber);
int getBlockToRead(int offset,int inode_number);

/* Global variables */
struct superblock_t sb;
int fd = 0;
int numberOfInodes;
int currentDirectoryInode = 1;
char *currentDirectoryName;

/***********************************************************************
 The main function:
    1) Lists the list of commands supported by the program
    2) Prompts the user to input command
    3) Reads the command and calls processCommand function 
		to process the given command
***********************************************************************/
int main()
{

	/* Array to store the list of commands */
	const char *a[7]; 
	a[0] = "initfs";
	a[1] = "load";
	a[2] = "cpin";
	a[3] = "cpout";
	a[4] = "mkdir";
	a[5] = "rm";
	a[6] = "q";
	
	currentDirectoryName =  malloc(100);

	strcpy(currentDirectoryName,"/");


	/* temp buffer to capture the extra \n */
	char tempbuffer[1];
	char command[100];
	int i;	
	printf("List of supported commands:\n\n");

	for(i=0;i<len(a);i++)
		printf("\t %d. %s\n",i+1,a[i]);

	while(1)
	{	
		printf("\n\nfsaccess %s$  ",currentDirectoryName);
		scanf("%[^\n]*c",command);
		scanf("%c",tempbuffer);
		if(processCommand(command))
			continue;
		else
			break;
	}
	if(fd!=0)
		close(fd);
	return 0;
}

/* To check if the file system is loaded or not */
int fileSystemLoaded()
{
	if(fd == 0)
	{
		printf("File system is not loaded. Please load or initialize file system!");
		return 0;
	}
	return 1;
}

/*******************************************************************************
 processCommand function:
    1) Reads the first part of the command and call the corresponding function 
    2) Returns 1 after exectuing the function, 0 if the command is q
********************************************************************************/
int processCommand(char cmd[100])
{
	int cmd_size = strlen(cmd);
	
	// strtok commands splits the command based on delimiter and gives the first substring
	char *cPtr = strtok(cmd,delimiter); 
	
	if(strcmp(cPtr,"initfs")==0)
		initfs(cPtr);
	else if(strcmp(cPtr,"load") == 0)
		load(cPtr);
	else if(strcmp(cPtr,"q")==0) // Saves the super block and returns 0
	{
			if(sb.fmod)
			{
				sb.fmod = 0;
				// Save super block and close the file
				lseek(fd, BLOCK_POSITION(1), SEEK_SET); 
				write(fd,&sb,sizeof(sb));	
			}
			return 0;
	}	
	else if (strcmp(cPtr,"cpin") == 0)
	{
		if(fileSystemLoaded())
			cpin(cPtr);
	}
	else if (strcmp(cPtr,"cpout") == 0)
	{
		if(fileSystemLoaded())
			cpout(cPtr);
	}	
	else if (strcmp(cPtr,"mkdir") == 0)
	{
		if(fileSystemLoaded())
			make_dir(cPtr);
	}
	else if (strcmp(cPtr,"rm") == 0)
	{
		if(fileSystemLoaded())
			rm(cPtr);
	}
	else if (strcmp(cPtr,"ls") == 0)
	{
		if(fileSystemLoaded())
			listDir();
	}
	else if (strcmp(cPtr,"cd") == 0)
	{
		if(fileSystemLoaded())
			changeParentDir(cPtr);
	}
	else 
		printf("Invalid command!");
	return 1;
}


/***********************************************************************
 initfs function:
    1) Splits the arguments and extracts fsize and number of inodes 
    2) Create Super block
	3) Initialize inodes
	4) Fill free Array and i-list
	5) Create Root Directory
	6) Write Super Block to the Disk
***********************************************************************/
void initfs(char *args){

	char* fileName;

	int i;
	args = strtok(NULL,delimiter);
	if(args == NULL){
		printf("Arguments missing!");
		return;
	}
	fileName = args;

	args= strtok(NULL,delimiter);
	if(args == NULL){
		printf("Arguments missing!");
		return;
	}
	sb.fsize = atoi(args);

	if(sb.fsize < 4){
		printf("Total number of blocks cannot be less than 4");
		return;
	}

	args= strtok(NULL,delimiter);
	if(args == NULL){
		printf("Arguments missing!");
		return;
	}

	numberOfInodes = atoi(args);
	
	// Calculate isize 
	sb.isize = ceil((double)numberOfInodes/(double)(NUMBER_OF_INODES_PER_BLOCK));
	// Open File
	fd = open(fileName,2);
	
	if(fd <=0)
	{
		printf("file %s does not exist. Create using touch command and try again",fileName);
		return;
	}

	DEBUG_LOG(("\n Creating super block..."));
	// Create super block
	
	sb.nfree = 0;
	DEBUG_LOG(("\n\t Add free data blocks to free[]..."));

	int first_D_Node_BlockNumber;
	int last_D_Node_BlockNumber;

	first_D_Node_BlockNumber = (2) + sb.isize;
	last_D_Node_BlockNumber = first_D_Node_BlockNumber + sb.fsize - 1 - 2 - sb.isize;
	
	// Adding block zero to free list this will be returned as the last free block, we can check for disk full case
	add_to_free_list(0); 
	
	/*Add free nodes to the free[]
	* Add the blocks in the reverse order
	* Root directory will be written on the first data block
	*/
	for(i=0;i<sb.fsize - 2 - sb.isize;i++) 
	{	
		add_to_free_list(last_D_Node_BlockNumber - i);
	}

	DEBUG_LOG(("\n\t Setting unallocated flag to all the inodes"));
	int j=0;
	//Set unallocated flags to all inodes and write to File
	for(i=1;i<=numberOfInodes;i++)
	{
			inode_t tempinode;
			tempinode.flags = 0;
			for(j=0;j<len(tempinode.addr);j++)
				tempinode.addr[j] = 0;
			tempinode.size0 = 0;
			tempinode.size1 = 0;
			lseek(fd,INODE_POSITION(i),SEEK_SET);
			write(fd,&tempinode,sizeof(tempinode));
	}

	sb.ninode = 0;

	DEBUG_LOG(("\n\t Add free inodes to i-list..."));
	
	//Add free inodes to i-list except the first node - First inode is reserved for root directory
	for(i=2;i <= len(sb.inode) && i <= (numberOfInodes);i++) // additional check in case isize is less than the array length
	{
			sb.inode[sb.ninode] = i;
			sb.ninode++;
	}

	
	DEBUG_LOG(("\n\t Creating Root directory"));
	// create directory in first datablock,
	// since we inserted the data blocks in the reverse order the firstDataBlockNumber should be passed
	int blockNumber = get_free_block();

    DEBUG_LOG("\n\t\t Creating root Directory on block:%d, First Data block Number:%d, Last Data block Number:%d",blockNumber,first_D_Node_BlockNumber,last_D_Node_BlockNumber);
	
	if(!blockNumber)
	 	return;

	// Create new Directory
	create_new_directory(blockNumber,1,1); 
	
	sb.flock = 0;
	sb.ilock = 0;
	sb.fmod = 1;
	
	//set time
	time_t sec;
	sec = time(NULL);

	sb.time[0] = sec >> 16;
	sb.time[1] = sec & (256*256 -1); 

    //Write super block to the file
	DEBUG_LOG(("\n\t Writing super block to the file"));
	
	// Seek to the current position from the begining of the disk
	lseek(fd, BLOCK_POSITION(1), SEEK_SET);
	write(fd,&sb,sizeof(sb));
	
	if(DEBUG)
	{
		print_free_inode_list();
		print_free_block_list();
	}

}


/***********************************************************************
 add_to_free_list function:
    1) Add the given block to the free list
	2) If free array is full, it will copy the free[] and nfree values into 
	   the passed block and stores the block number as the first element in the free[]
***********************************************************************/
void add_to_free_list(int blockNumber)
{
	
	if(sb.nfree < len(sb.free))
	{
		sb.free[sb.nfree] = blockNumber;
		sb.nfree++;
	}
	else
	{
		int i;
		
		//create a first free block with free[] and nfree
		firstfreeblock_t freeBlock;
		freeBlock.nfree = sb.nfree;
		
		//copy free[]
		for(i=0;i<sb.nfree;i++)
		{
			freeBlock.free[i] = sb.free[i];
		}
		
		sb.nfree = 0;
	    sb.free[sb.nfree] = blockNumber;
		sb.nfree++;	

		//write to data block
		lseek(fd,BLOCK_POSITION(blockNumber),SEEK_SET);
		write(fd,&freeBlock,sizeof(freeBlock));
		
	}

	sb.fmod = 1;
}

/***********************************************************************
 get_free_block function:
    1) Fetches a free block from the free[]
	2) if nfree is greater than 0, retunr the block number from the free array.
	   free[0] holds the next nfree and free[] values, store them in super block's free[]
	   before returning the block for allocation
***********************************************************************/
unsigned int get_free_block()
{
	int newblock;
	sb.nfree--;
	if(sb.nfree!=0)
	{
		newblock = sb.free[sb.nfree];
	}
	else
	{
		newblock = sb.free[sb.nfree];
		if(newblock!=0)  // if new block is 0 then there is no more blocks to allocate
		{
			//copy the contents of free[0] to nfree and free[]
			int i;

			firstfreeblock_t freeBlock;
			lseek(fd,BLOCK_POSITION(newblock),SEEK_SET);
			read(fd,&freeBlock,sizeof(freeBlock));
			
			
			sb.nfree = freeBlock.nfree;
			for(i=0;i<sb.nfree;i++)
			{
				sb.free[i] = freeBlock.free[i];
			}
		}
		else
		{	
			sb.nfree++;
			DEBUG_LOG("\nNo more blocks to allocate!");
		}
	}
	sb.fmod = 1;
	return newblock; 
}
/***********************************************************************
 create_new_directory function:
    1) Writes . and .. entries to the data block of the directory
	2) Create a new inode for the directory and write to the disk
***********************************************************************/
void create_new_directory(int blockNumber,int parentinode,int newinode)
{
	DEBUG_LOG(("\n\t\t Creating directory in data block"));
	//write the directory to data block
	lseek(fd,BLOCK_POSITION(blockNumber),SEEK_SET);
	directoryitem_t dir;
	dir.inode = newinode;
	strcpy(dir.name,".");

	DEBUG_LOG(("\n\t\t writing directory entry (..) to data block"));
	write(fd,&dir,sizeof(dir));
	
	directoryitem_t parentDir;
	dir.inode = parentinode;
	strcpy(dir.name,"..");
	
	DEBUG_LOG(("\n\t\t writing directory entry (.) to the same data block"));
	write(fd,&dir,sizeof(parentDir));

	DEBUG_LOG(("\n\t\t Creating new inode for the directory"));
	//Add the directory info to the inode
	inode_t inode;
	inode.flags =0;
	inode.flags = inode.flags | (1 << 15); // set allocation
	inode.flags = inode.flags | (1 << 14); //  directory allocation 
	inode.flags = inode.flags & (6 << 13); //  
	inode.flags = inode.flags & (14 << 12); // small file

	//set read write execute permissions
	inode.flags = inode.flags | (1 << 8); // read
	inode.flags = inode.flags | (1 << 7); // write
	inode.flags = inode.flags | (1 << 6); //  execute

	inode.nlinks = 1;
	inode.uid = 0;
	inode.gid = 0;
	
	int totSizeOfDir = (int)(sizeof(dir) + sizeof(parentDir));
    
	inode.size0 = totSizeOfDir >> 16;
	inode.size1 = totSizeOfDir & (256*256 -1);

	int j;
	for(j=0;j<len(inode.addr);j++)
		inode.addr[j] = 0;

	inode.addr[0] = blockNumber; // adding the data block to the list of addresses

	time_t sec;
	sec = time(NULL);

	inode.acttime[0] = sec >> 16;
	inode.acttime[1] = sec & (256*256 -1); 

	inode.modtime[0] = sec >> 16;
	inode.modtime[1] = sec & (256 * 256 -1); 
	
	DEBUG_LOG(("\n\t\t Writing new inode to file"));
	//Write the directory inode to file

	lseek(fd,INODE_POSITION(newinode),SEEK_SET);
	write(fd,&inode,sizeof(inode));
}

/* Function that add the inode entry to parent directory
   name - name of the file/directory to be added
   parentinode - inode number of the parent directory 
   newinode -the inode number of the file/directory to be added
*/
void add_directoryEntry_to_parentDir(char *name,int parentinode,int newinode)
{
	short isAllocated = 0;
	short isDirectory = 0;
	inode_t parent_inode;
	
	//Create Directory Entry
	directoryitem_t dir;
	strcpy(dir.name,name);
	dir.inode = newinode;

	//read parent inode
	lseek(fd,INODE_POSITION(parentinode),SEEK_SET);
	read(fd,&parent_inode,sizeof(parent_inode));

	//Check isAllocated
	isAllocated = (parent_inode.flags >> 15); // 1st bit

	//Check isDirectory
	isDirectory = ((parent_inode.flags & (1 << 14)) >> 14); // 2nd bit

	if(!(isAllocated & isDirectory))
	{
		printf("Invalid Operation: Not a directory");
		return;
	}

	int dirSize = parent_inode.size0 << 16 | parent_inode.size1;
	int offset = dirSize % BLOCK_SIZE;

	int blockNumber = getBlockToRead(dirSize,parentinode);

	if(blockNumber == 0) //empty block, no space in the datablocks of current inode
	{
		blockNumber = get_free_block();
		if(blockNumber)
			addDataBlockToInode(parentinode,blockNumber); // Add new block to the inode
		else
			return; // No more blocks to allocate in the entire file system
	}

	// write the directory entry
	lseek(fd,BLOCK_POSITION(blockNumber) + offset,SEEK_SET);
	write(fd,&dir,sizeof(dir));
	
	//change directory size
	dirSize = dirSize + sizeof(dir);
	parent_inode.size0 = dirSize >> 16;
	parent_inode.size1 = dirSize & (256*256 -1);

	//write changes to the parentinode
	lseek(fd,INODE_POSITION(parentinode),SEEK_SET);
	write(fd,&parent_inode,sizeof(parent_inode));
}



/***********************************************************************
 get_free_inode function:
    1) if ninode is greater than 0, returns the free i-number from the i-listand decrements the ninode
	 	else loops through all the i nodes and fills the i-list by checking the allocated flag of each inode
		 			tries again after filling
***********************************************************************/
unsigned int get_free_inode(){
	sb.fmod = 1;
	if(sb.ninode > 0)
	{
		sb.ninode--;
		return sb.inode[sb.ninode];
	}
	else // if there is no inode loop through the i list and copy unallocated inodes to inode[]
	{	
		int i;

		inode_t tempinode;
		for(i=2;i<=(numberOfInodes) && sb.ninode < len(sb.inode);i++)
		{
			lseek(fd,INODE_POSITION(i),SEEK_SET);
			short isAllocated = 0;
			read(fd,&tempinode,sizeof(tempinode));
			isAllocated = (tempinode.flags >> 15); // 1st bit
			if(isAllocated == 0){
				sb.inode[sb.ninode] = i;
				sb.ninode++;
			}
		}
		if(sb.ninode == 0)
			return 0; // all i-nodes are allocated
		else 
			return get_free_inode(); // try again to get inode
	}
}

/***********************************************************************
 add_free_inode function:
    Adds the passed i-number to the i-list if it is not full
***********************************************************************/
void add_free_inode(int inumber)
{
	sb.fmod = 1;
	if(sb.ninode < len(sb.inode) && (sb.ninode < numberOfInodes))
	{
		sb.inode[sb.ninode] = inumber;
		sb.ninode++;
	}
}


/***********************************************************************
/* loads the existing filesystem 
***********************************************************************/
void load(char *args)
{
	char* fileName;
	
	int i;
	args = strtok(NULL,delimiter);
	if(args == NULL){
		printf("Arguments missing!");
		return;
	}
	fileName = args;

	// Save existing changes before loading new file system
	if(fd!=0)
	{
		if(sb.fmod)
			{
				sb.fmod = 0;
				// Save super block and close the file
				lseek(fd, BLOCK_POSITION(1), SEEK_SET); 
				write(fd,&sb,sizeof(sb));	
			}
	}

	// open the file
	fd = open(fileName,2);

	//read super block 
	lseek(fd, BLOCK_POSITION(1), SEEK_SET);
	read(fd,&sb,sizeof(sb));

	//read number of inodes from super block
	numberOfInodes = sb.isize * NUMBER_OF_INODES_PER_BLOCK;

	if(DEBUG)
	{
		print_free_inode_list();
		print_free_block_list();
	}
}

/* Function to check if a file exists in the directory
 *	filename- name of the file
 *	parent_inode_number - inode number of the directory	
*/
int fileExists(char *fileName, int parent_inode_number)
{
	int i;
	int noOfitems=0;
	
	directoryContent* list = getDirectoryContents(&noOfitems,parent_inode_number);

	//Loop through all the contents of the directory and return the inode of the file if it exists	
	for(i=0;i<noOfitems;i++)
	{	
		if(strcmp(list[i].name,fileName)==0)
		{
			return list[i].inode;
		}
	}
	return 0;
}

/***************************************************************************************
 *  function that returns a list of contents in the directory
	noOfItems - refernece parameter, refers to the number of items in the directory
	inode_number - inode number of the directory	
 *******************************************************************************************/
directoryContent* getDirectoryContents(int* noOfitems,int inode_number)
{

	DEBUG_LOG("\n\t Getting contents of dir Inode: %d",inode_number);
	inode_t directoryInode;
	inode_t tempInode;

	short isAllocated = 0;
	short isDirectory = 0;
	int i=0,j=0;

	//Read i-node
	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	read(fd,&directoryInode,sizeof(directoryInode));

	//Check isAllocated
	isAllocated = (directoryInode.flags >> 15); // 1st bit
	directoryContent* list;

	//Check isDirectory
	isDirectory = ((directoryInode.flags & (1 << 14)) >> 14); // 2nd bit
	
	if(isAllocated & isDirectory)
	{
		//Get the size of the directory
		int sizeToRead = directoryInode.size0 << 16 | directoryInode.size1;
		int totalbytesReadPerBlock =0;
		int totalbytesRead = 0;
		int blockNumber = 0;

		//Calculate the number of items based on directory size
		*noOfitems = sizeToRead/sizeof(directoryitem_t);	
		//Allocate memory for the directory
		list = malloc(sizeof(directoryContent) * (*noOfitems));

		int bytesread =0;
		blockNumber=getBlockToRead(totalbytesRead,inode_number);
		//while all the blocks are read or until the sizeToRead becomes 0 
		while(sizeToRead!=0 && blockNumber!=0) // --> gets the block based on offset
		{
			//read the directory item from the block
			directoryitem_t dir;
			lseek(fd,BLOCK_POSITION(blockNumber),SEEK_SET);
			totalbytesReadPerBlock = 0;

			//Read until the end of the directory or till the sizeToRead becomes 0		
			while(sizeToRead!=0 && totalbytesReadPerBlock != BLOCK_SIZE)
			{
				bytesread = read(fd,&dir,sizeof(dir));
				totalbytesReadPerBlock += bytesread;
				totalbytesRead += bytesread;
				sizeToRead -= bytesread; 

				// If the file/directory is not deleted
				if(dir.inode!=0)
				{
					//Add to list
					list[j].inode = dir.inode;
					strcpy(list[j].name,dir.name);
					j++;
				}

			}
			blockNumber=getBlockToRead(totalbytesRead,inode_number);

		}

		// update number of items added to the list
		*noOfitems = j;

		//updating file size and isDirectory to the list
		for(j=0;j<*noOfitems;j++)
		{
			//read each inode in the list and update the file size and file type
			lseek(fd,INODE_POSITION(list[j].inode),SEEK_SET);
			read(fd,&tempInode,sizeof(tempInode));
			list[j].isDirectory = ((tempInode.flags & (1 << 14)) >> 14);
			list[j].fileSize = tempInode.size0 << 16 | tempInode.size1;; 
		}
		return list;
	}
	return NULL;
}

// function to convert small file into large file
void makeLargefile(int inode_number)
{
	inode_t currentInode;
	int i=0,j=0;
	DEBUG_LOG("\nMaking Large File");
	
	//Read i-node
	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	read(fd,&currentInode,sizeof(currentInode));

	// make sure the given file is small file
	short isLargeFile = ((currentInode.flags & (1 << 12)) >> 12); 
	if(!isLargeFile)
	{
		currentInode.flags = currentInode.flags | 1 << 12; // Set as large file
		int singleIndirectionblockNumber = get_free_block();
		
		//create a single indirect block
		singleIndirectblock_t sib;
				DEBUG_LOG("\nSingle indirect block number : %d",singleIndirectionblockNumber);
		//Take  every element of the addr[] and add it to single indirection block
		for(i=0;i<len(currentInode.addr);i++)
		{
			if(currentInode.addr[i]!=0)
			{
				DEBUG_LOG("\nAdded %d to block %d at pos %d" ,currentInode.addr[i], singleIndirectionblockNumber,j);
				
				sib.blockNumbers[j++] = currentInode.addr[i];
				
			}	
			currentInode.addr[i] = 0; // Set it to 0, to mark as empty
		}

		//make other block numbers in single indirect block to 0
		for(;j<len(sib.blockNumbers);j++)
			sib.blockNumbers[j] = 0;

		// write single indirect block
		lseek(fd,BLOCK_POSITION(singleIndirectionblockNumber),SEEK_SET);
		write(fd,&sib,sizeof(sib)); 

		//Add the single indirection block to addr[0]
		currentInode.addr[0] = singleIndirectionblockNumber;

		// write the i-node
		lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
		write(fd,&currentInode,sizeof(currentInode));
	}

}

/* function to create a directory */
/********************************************************************************************
/* args - directory name
	check if the directory already exists, if not creates a directory recursively */
/********************************************************************************************/
void make_dir(char *args)
{
	char* dirName;
	
	int i;
	args = strtok(NULL,delimiter);
	if(args == NULL){
		printf("Arguments missing!");
		return;
	}

	
	int parent_inode;
	int new_inode =0;
	dirName = args;

	dirName = strtok(dirName,"/"); // Split on '/'

	if(dirName[0] == '/')
		parent_inode = 1;  //Initialize parent directory to root
	else 
		parent_inode = currentDirectoryInode;

	inode_t currentInode;

	while(dirName) // recursively check each part of the directory path
	{
		if(new_inode = fileExists(dirName,parent_inode)) // If directory already exists
		{
		
			// Check if it is a directory
			lseek(fd,INODE_POSITION(new_inode),SEEK_SET);
			read(fd,&currentInode,sizeof(currentInode));
			short isDirectory = ((currentInode.flags & (1 << 14)) >> 14); // 2nd bit
			
			if(isDirectory)  // if the directory already exists, change the parent directory and go to next token
			{
				printf("\nDirectory '%s' exists",dirName);
				parent_inode = new_inode; // Change the parent directory
				dirName = strtok(NULL,"/"); // split on '/', create a directory for every split
				continue;
			}
		}
			new_inode = get_free_inode(); // new inode for the new directory
			
			int blockNumber = get_free_block(); // get free data block to store the directory contents
			if(blockNumber == 0)
				return;
			DEBUG_LOG("\n Creating new directory: '%s'",dirName);
			
			create_new_directory(blockNumber,parent_inode,new_inode);
			add_directoryEntry_to_parentDir(dirName,parent_inode,new_inode);

			printf("\n Created directory: '%s'",dirName);

			parent_inode = new_inode; // change the parent inode to the new directory

			dirName = strtok(NULL,"/"); // split on '/', create a directory for every split
	}
}

/* *****************************************************************************************
 * Removes the file/directory from the file system
 * Search for the file in file system and deletes the file 
 * ****************************************************************************************/
void rm(char *args)
{
	char* filePath;
	
	int i;

	args = strtok(NULL,delimiter);
	if(args == NULL){
		printf("Arguments missing!");
		return;
	}

	filePath = args;
	// Set root directory as current inode
	int currentInode = 1;
	
	if(filePath[0] == '/')
		currentInode = 1;  //Initialize parent directory to root
	else 
		currentInode = currentDirectoryInode;
	
	inode_t current_Inode;
	int found_inode = 0;
	short isDirectory;
	
	filePath = strtok(filePath,"/"); // Split on '/'
	
	// traverses inside the file path till end to get the inode
	while(filePath)
	{
		if(found_inode = fileExists(filePath,currentInode)) // If file exists
		{
			// Check if it is a directory
			lseek(fd,INODE_POSITION(found_inode),SEEK_SET);
			read(fd,&current_Inode,sizeof(current_Inode));
			isDirectory = ((current_Inode.flags & (1 << 14)) >> 14); // 2nd bit
			
			if(isDirectory)
			{
				currentInode = found_inode; // Change the parent directory
				filePath = strtok(NULL,"/"); // split on '/'
				continue;
			}
		}
		else
		{
			printf("\nNo such file/directory exists!");
			return;
		}
		filePath = strtok(NULL,"/");
	}

	
	isDirectory = ((current_Inode.flags & (1 << 14)) >> 14); // 2nd bit
	
	if(isDirectory) // if the found inode is directory 
	{
		deleteDir(found_inode);
	}
	else
	{
		deleteDirectoryEntry(currentInode,found_inode);
		deleteFile(found_inode);
	}
}

/* Deletes the directory entry in the parent inode for the given inode number */
void deleteDirectoryEntry(int parent_inode_number,int inode_number)
{

	if(inode_number == 1)
	{
		printf("Error! Cannot delete root directory!");
		return;
	}

	short isAllocated = 0;
	short isDirectory = 0;
	inode_t parent_inode;
	int i,j,k;

	//Create Directory Entry
	directoryitem_t dir;

	lseek(fd,INODE_POSITION(parent_inode_number),SEEK_SET);
	read(fd,&parent_inode,sizeof(parent_inode));

	//Check isAllocated
	isAllocated = (parent_inode.flags >> 15); // 1st bit

	//Check isDirectory
	isDirectory = ((parent_inode.flags & (1 << 14)) >> 14); // 2nd bit

	if(!(isAllocated & isDirectory))
	{
		printf("Invalid Operation: Not a directory");
		return;
	}

	int sizeToRead = parent_inode.size0 << 16 | parent_inode.size1;
	int totalbytesReadPerBlock =0;
	int totalbytesRead = 0;

	//Find Data block to delete
	int blockNumber=getBlockToRead(totalbytesRead,parent_inode_number);
	int bytesread =0;

	while(sizeToRead!=0 && blockNumber!=0)
		{
			//read the directory item from the block
			directoryitem_t dir;
			lseek(fd,BLOCK_POSITION(blockNumber),SEEK_SET); 
			totalbytesReadPerBlock = 0;

			//Read until the end of the directory block (1024 bytes/1 BLOCK) or till the sizeToRead becomes 0		
			while(sizeToRead!=0 && totalbytesReadPerBlock != BLOCK_SIZE)
			{
				bytesread = read(fd,&dir,sizeof(dir));
				totalbytesReadPerBlock += bytesread;
				totalbytesRead += bytesread;
				sizeToRead -= bytesread; 

				// If the inode number matches the file inode number to be deleted
					if(dir.inode == inode_number)
					{
						DEBUG_LOG("\nDeleting %s",dir.name);
						dir.inode = 0; // by  making inode as 0 we are unlinking the file from the directory
						lseek(fd,-bytesread,SEEK_CUR);
						write(fd,&dir,sizeof(dir));
						printf("\nDeleted '%s'",dir.name);
						return;
					}

			}
			blockNumber=getBlockToRead(totalbytesRead,parent_inode_number);  // --> gets the block based on offset
		}	
}

/* Deletes the directory based on inode_number */
void deleteDir(int inode_number)
{
	inode_t currentInode;
	int i,j,k;
	
	int noOfitems=0;
	directoryContent* directoryContents = getDirectoryContents(&noOfitems,inode_number);
	int parent_directoryInode = 1;
	for(i=0;i<noOfitems;i++)
	{	// Delete contents other than . and .. as its the directory itself and the parent directory
		
		if(strcmp(directoryContents[i].name,"..") == 0 )
		{
			parent_directoryInode = directoryContents[i].inode; // storing parent directory inode to remove the deleted directory entry from its parent
		}
		else if(strcmp(directoryContents[i].name,".") !=0 &&
					strcmp(directoryContents[i].name,"..") !=0 )
					{
			if(directoryContents[i].isDirectory)
			{
				deleteDir(directoryContents[i].inode);
			}
			else
			{
				deleteDirectoryEntry(inode_number,directoryContents[i].inode);
				deleteFile(directoryContents[i].inode);
			}
		}
	}

	deleteDirectoryEntry(parent_directoryInode,inode_number);

}

/* Function to delete the file */
void deleteFile(int inode_number)
{
	inode_t currentInode;
	int i,j,k;
	
	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	read(fd,&currentInode,sizeof(currentInode));

	short isLargeFile = ((currentInode.flags & (1 << 12)) >> 12); 
	
	currentInode.flags =0; // set as unallocated inode

	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	write(fd,&currentInode,sizeof(currentInode));

	int singleIndirectionblockNumber;

	if(isLargeFile)
	{
		// Single Indirection 
		for(i=0;i<len(currentInode.addr) - 1 && currentInode.addr[i] !=0;i++)
		{
			singleIndirectionblockNumber = currentInode.addr[i];
			singleIndirectblock_t sib;
			lseek(fd,BLOCK_POSITION(singleIndirectionblockNumber),SEEK_SET);
			read(fd,&sib,sizeof(sib));
			
			// read all the block numbers from the single indirection block and add it to free list
			for(j=0;j<len(sib.blockNumbers);j++)
			{
				if(sib.blockNumbers[j]!=0)
					add_to_free_list(sib.blockNumbers[j]);
			}
			add_to_free_list(currentInode.addr[i]);
			currentInode.addr[i] = 0;
		}

		// Triple Indirection
		if(currentInode.addr[len(currentInode.addr) - 1] != 0)
		{
			// Read the last block of addr[] 
			singleIndirectblock_t sib1; // first level of triple indirection
			lseek(fd,BLOCK_POSITION(currentInode.addr[len(currentInode.addr) - 1]),SEEK_SET);
			read(fd,&sib1,sizeof(sib1));
			
			for(i=0;i<len(sib1.blockNumbers);i++)
			{
				if(sib1.blockNumbers[i]!=0)
				{
					singleIndirectblock_t sib2; // second level of triple indirection
					lseek(fd,BLOCK_POSITION(sib1.blockNumbers[i]),SEEK_SET);
					read(fd,&sib2,sizeof(sib2));
					
					for(j=0;j<len(sib2.blockNumbers);j++)
					{
						if(sib2.blockNumbers[j]!=0)
						{
							singleIndirectblock_t sib3; // third level of triple indirection
							lseek(fd,BLOCK_POSITION(sib2.blockNumbers[j]),SEEK_SET);
							read(fd,&sib3,sizeof(sib3));

							for(k=0;k<len(sib3.blockNumbers);k++)
							{
								if(sib3.blockNumbers[k]!=0)
									add_to_free_list(sib3.blockNumbers[k]);
							}
							add_to_free_list(sib2.blockNumbers[j]);
						}
					}
					add_to_free_list(sib1.blockNumbers[i]);
				}
			}
		}
	}
	else
	{
		for(i=0;i<len(currentInode.addr) && currentInode.addr[i] !=0;i++)
		{
			add_to_free_list(currentInode.addr[i]);
			DEBUG_LOG("\nAdding %d to free list",currentInode.addr[i]) ;
			currentInode.addr[i] = 0;
		}
	}

	//Updating file size to 0
	currentInode.flags = 0;
	currentInode.size0 = 0;
	currentInode.size1 = 0;

	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	write(fd,&currentInode,sizeof(currentInode));
	 
	//Add to free i-list
	add_free_inode(inode_number);
}

//Splits and returns the last part of the filepath
char* getfileName(char* filePath)
{
	char*  fileName;
	filePath = strtok(filePath,"/");
	while(filePath)
	{	
		fileName = filePath;
		filePath = strtok(NULL,"/"); 
	}

	return fileName;
}

// function to change the parentDir
void changeParentDir(char *args)
{
	char *filePath;
	char *newPath;
	char *defaultName;
	args = strtok(NULL,delimiter);
if(args == NULL){
		printf("Arguments missing!");
		return;
	}

	filePath = args;
	short moveToParent =0;

	if(filePath[0] == '/')
	{	
		currentDirectoryInode = 1;
		newPath = malloc(strlen(filePath)+2);
		newPath[0] ='\0';
		strcpy(newPath,filePath);
	}
	else
	{
		if(strcmp(filePath,"..")!=0 
					&& strcmp(filePath,".")!=0)
		{
			newPath = malloc(strlen(filePath)+strlen(currentDirectoryName)+2);
			newPath[0] ='\0';
			strcat(newPath,currentDirectoryName);
			if(currentDirectoryInode!=1)
				strcat(newPath,"/");
			strcat(newPath,filePath);
		}
	}

	int parentdir = currentDirectoryInode;

	if(strcmp(filePath,"..") == 0)
		moveToParent = 1;
	
	// recursively traverses inside the parent directory to find the path
	int foundInode = searchFileInFileSystem(filePath,&parentdir,defaultName);
	
	if(foundInode > 0)
	{
		currentDirectoryInode = foundInode;
		if(currentDirectoryInode !=1)
		{
			if(moveToParent == 1)
			{
				newPath = malloc(strlen(currentDirectoryName)+2);
				newPath[0] ='\0';
				char*  fileName;
				
				fileName = strtok(currentDirectoryName,"/");
	
				while(fileName)
				{

					char *tempPart = malloc(strlen(fileName));

					strcpy(tempPart,fileName);
					fileName = strtok(NULL,"/"); 
					if(fileName)
					{
						strcat(newPath,"/");
						strcat(newPath,tempPart);
					}
				}
			}
		
			if(newPath)
				strcpy(currentDirectoryName,newPath);
		}
	}
	else
	{
		printf("\nInvalid directory path");
	}
	
	if(currentDirectoryInode==1)
			strcpy(currentDirectoryName,"/");

}
// prints the list of  directory contents
void listDir()
{
	int noOfitems=0,i;
	directoryContent* list = getDirectoryContents(&noOfitems,currentDirectoryInode);
	
	for(i=0;i<noOfitems;i++)
	{
		if(list[i].isDirectory)
			printf("\n %s\t dir",list[i].name);
		else
			printf("\n %s\t file \t %d",list[i].name,list[i].fileSize);
	}
}

/***************************************************************
 * Function to read external file and writing to v6 file system 
 * 
 * args split by a space in between
 * 			"external file path" "v6 file path"
 * ***************************************************************/
void cpin(char* args)
{
	char* extfileName;
	char* v6fileName;

	// reads the external file and writes 1 byte at a time using read buffer
	char readbuffer;
	

	//split the arguments by space to get v6 file path and ext file path
	args = strtok(NULL,delimiter);

	if(args == NULL){
		printf("Arguments missing!");
		return;
	}
	
	extfileName = args;

	args= strtok(NULL,delimiter);
	if(args == NULL){
		printf("Arguments missing!");
		return;
	}
	
	v6fileName = args;
	
	int efd =0;
	// Open external file in read mode
	efd = open(extfileName,0); //read mode
	
	if(efd <=0)
	{
		printf("File %s does not exist.",extfileName);
		return;
	}

	char *targetFileName, *v6filePath;
	
	//copying v6 filepath to a temp variable
	v6filePath =  malloc(sizeof(v6fileName));
	strcpy(v6filePath,v6fileName);
	
	//Gets the last part of the v6 filepath. (delimited by '/')
	targetFileName = getfileName(v6filePath);

	int parent_inode_number = 1;
	int inode_number = 0;

	// Update parent inode based on the input v6 file path (with reference to current directory or with respect to root)
	if(v6fileName[0] =='/')
		parent_inode_number = 1;
	else
		parent_inode_number = currentDirectoryInode;
	
	inode_t fileInode;
	
	int isDirectory;

	//Get the inode number of the file in file system along with the parent inode number If exists, delete the file
		//If the target v6 file is already present, the inode number is returned otherwise 0
	if(inode_number = searchFileInFileSystem(v6fileName,&parent_inode_number,targetFileName))
	{
		if(inode_number == -1)
		{
			printf("Invalid v6 file path. Create directory path using mkdir and try again!");
			return;
		}
		
			// Check if it is a directory
			lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
			read(fd,&fileInode,sizeof(fileInode));
			isDirectory = ((fileInode.flags & (1 << 14)) >> 14); // 2nd bit
			
			if(isDirectory)
			{
				printf("Error! Directory exists with the same name!");
				return;
			}

		printf("File %s already exist. Overwriting file...",targetFileName);
		deleteDirectoryEntry(parent_inode_number,inode_number);
		deleteFile(inode_number);
	}
	
	//Create Inode for the file
	inode_number = get_free_inode();
	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	read(fd,&fileInode,sizeof(fileInode));
	

	fileInode.flags = fileInode.flags | (1 << 15); // set allocation
	fileInode.flags = fileInode.flags & (0 << 14); // set as file

	//set size as 0
	fileInode.size0 = 0;
	fileInode.size1 = 0;
	
	
	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	write(fd,&fileInode,sizeof(fileInode));
	
	int bytesRead = 0;
	int bytesWrittenPerBlock = 0;
	int blockNumber;
	int fileSize = 0;
	 
	
	int isLargeFile = 0; 
	// Write contents of the file into data blocks and add it to inode 
		// reads the external file and writes 1 byte at a time using read buffer
	while(bytesRead = read(efd,&readbuffer,sizeof(readbuffer)))
	{
		int offset;
		
		offset = fileSize % BLOCK_SIZE;
		
		//when the current blocks becomes full
		if(bytesWrittenPerBlock == 0)
		{
			//Get a free block
			blockNumber = get_free_block();
			DEBUG_LOG("\n Writing to block number %d, %d",blockNumber ,fileInode.size1);
			if(blockNumber)
				addDataBlockToInode(inode_number,blockNumber); // Adding the block to addr[] based on the file size
			else
				return;
  		}
		
			
		lseek(fd,BLOCK_POSITION(blockNumber) + offset,SEEK_SET);
		write(fd,&readbuffer,sizeof(readbuffer));
		fileSize += bytesRead;

		// To reset the offset and check if the current block is full, bytesWrittenPerBlock becomes 0 when the current block is full
		bytesWrittenPerBlock = (bytesWrittenPerBlock + bytesRead) % BLOCK_SIZE;
	
		lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
		read(fd,&fileInode,sizeof(fileInode));
		
		fileInode.size0 = fileSize >> 16;
		fileInode.size1 = fileSize & (256*256 -1);
	
		lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
		write(fd,&fileInode,sizeof(fileInode));
	}

	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	read(fd,&fileInode,sizeof(fileInode));
	
	//update file size
	fileInode.size0 = fileSize >> 16;
	fileInode.size1 = fileSize & (256*256 -1);

	time_t sec;
	sec = time(NULL);

	//Update modified time
	fileInode.acttime[0] = sec >> 16;
	fileInode.acttime[1] = sec & (256*256 -1); 

	fileInode.modtime[0] = sec >> 16;
	fileInode.modtime[1] = sec & (256 * 256 -1); 

	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	write(fd,&fileInode,sizeof(fileInode));
	
	add_directoryEntry_to_parentDir(targetFileName,parent_inode_number,inode_number);
}


/***************************************************************
 * Function to copy from v6 to external file
 * 
 * args split by a space in between
 * 			"v6 file path" "external file path" 
 * ***************************************************************/
void cpout(char* args)
{
	char* extfileName;
	char* v6fileName;

	// reads the external file and writes 1 byte at a time using read buffer
	char readBuffer;

	int parent_inode_number = 1;

	//split the arguments by space to get v6 file path and ext file path
	args = strtok(NULL,delimiter);
	if(args == NULL){
		printf("Arguments missing!");
		return;
	}
	
	v6fileName = args;

	args= strtok(NULL,delimiter);
	if(args == NULL){
		printf("Arguments missing!");
		return;
	}
	
	extfileName = args;
	int efd =0;

	efd = creat(extfileName,2); //read-write mode

	if(efd<0)
	{
		printf("Cannot create external file. Please check the file Path");
		return;
	}
	
	char *fileName, *v6filePath;
	
	v6filePath =  malloc(sizeof(v6fileName));
	strcpy(v6filePath,v6fileName);
	
	// Gets the last part of the passed argument, file name of the v6
	fileName = getfileName(v6filePath);


	//change parent directory
	if(v6fileName[0] =='/')
		parent_inode_number = 1;
	else
		parent_inode_number = currentDirectoryInode;
	
	// search for the inode number in v6 file system 
	int found_inode = searchFileInFileSystem(v6fileName,&parent_inode_number,fileName);

	if(found_inode <=0)
	{
		printf("File %s does not exist.",fileName);
		return;
	}

	inode_t fileInode;
	
	//Read the file
	lseek(fd,INODE_POSITION(found_inode),SEEK_SET);
	read(fd,&fileInode,sizeof(fileInode));
	char buffer;
	int bytesToRead = fileInode.size0 << 16 | fileInode.size1;
	int fileOffset = 0;

	while(bytesToRead) // Total file size left to read
	{
			int blockNumber = getBlockToRead(fileOffset,found_inode);
			DEBUG_LOG("\n reading from block number %d ",blockNumber);
			// Read from the block and write to file
			lseek(fd,BLOCK_POSITION(blockNumber),SEEK_SET);

			int bytesToReadPerBlock = BLOCK_SIZE;
			//Read the block till BLOCK_SIZE
			while(bytesToRead && bytesToReadPerBlock)
			{
				int bytesRead = read(fd,&buffer,sizeof(buffer));
				write(efd,&buffer,sizeof(buffer));
				bytesToRead -= bytesRead;
				bytesToReadPerBlock -=bytesRead;
				fileOffset +=bytesRead;
			}
	}	
	
	close(efd);
}

//Gets the block from file-inode based on the offset
int getBlockToRead(int offset,int inode_number)
{
	inode_t fileInode;
	int i;
	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	read(fd,&fileInode,sizeof(fileInode));
	short isLargeFile = ((fileInode.flags & (1 << 12)) >> 12);

	DEBUG_LOG("\n Get Block to read, Offest %d ",offset);
	int logicalBlockNumber = offset/BLOCK_SIZE;

	if(!isLargeFile) // small file
	{
		return fileInode.addr[logicalBlockNumber];
	}
	else // large file
	{
		DEBUG_LOG("\n\t Large file read, logicalBlockNumber %d ",logicalBlockNumber);
		
		int singleIndirectionblockNumber = (logicalBlockNumber/NUMBER_OF_BLOCKS_PER_INDIRECTION);
		DEBUG_LOG("\n\t single Indirection logical block Number :%d ",singleIndirectionblockNumber);
		
		//First ten blocks are single indirection blocks
		if(singleIndirectionblockNumber < len(fileInode.addr)-1)
		{
			singleIndirectblock_t sib;
			lseek(fd,BLOCK_POSITION(fileInode.addr[singleIndirectionblockNumber]),SEEK_SET);
			read(fd,&sib,sizeof(sib));
			return sib.blockNumbers[logicalBlockNumber%NUMBER_OF_BLOCKS_PER_INDIRECTION];
		}
		else{ // triple indirection
			
			//Any logical number greater than 10 will present inside the last position of addr[] in triple indirection
			singleIndirectionblockNumber = len(fileInode.addr) -1;
			singleIndirectblock_t sib1;

			//travesrsing through triple indirection
			lseek(fd,BLOCK_POSITION(fileInode.addr[len(fileInode.addr)-1]),SEEK_SET);
			read(fd,&sib1,sizeof(sib1));

			int remainingBlocks = logicalBlockNumber - (NUMBER_OF_BLOCKS_PER_INDIRECTION * (len(fileInode.addr)-1));
			int tripleIndirectionLogicalBlockNumber = (remainingBlocks/(NUMBER_OF_BLOCKS_PER_INDIRECTION
																	         *NUMBER_OF_BLOCKS_PER_INDIRECTION));
			int tripleIndirectionBlockNumber = sib1.blockNumbers[tripleIndirectionLogicalBlockNumber];

			//travesrsing through double indirection
			singleIndirectblock_t sib2;
			lseek(fd,BLOCK_POSITION(tripleIndirectionBlockNumber),SEEK_SET);
			read(fd,&sib2,sizeof(sib2));

			int doubleIndirectionLogicalBlockNumber = (remainingBlocks/NUMBER_OF_BLOCKS_PER_INDIRECTION);
			int doubleIndirectionBlockNumber = sib2.blockNumbers[doubleIndirectionLogicalBlockNumber];

			//travesrsing through single indirection
			singleIndirectblock_t sib3;
			lseek(fd,BLOCK_POSITION(doubleIndirectionBlockNumber),SEEK_SET);
			read(fd,&sib3,sizeof(sib3));

			
			int singleIndirectionBlockNumber = (remainingBlocks%NUMBER_OF_BLOCKS_PER_INDIRECTION);
			return sib3.blockNumbers[singleIndirectionBlockNumber];
		}

	}
}

//Adds the block to end of the file inode
void addDataBlockToInode(int inode_number,int blockNumber)
{
	inode_t fileInode;
	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	read(fd,&fileInode,sizeof(fileInode));

	int logicalBlockNumber;
	
	 int i;
	short isLargeFile = ((fileInode.flags & (1 << 12)) >> 12);
	int fileSize = fileInode.size0 << 16 | fileInode.size1;

	logicalBlockNumber = fileSize / BLOCK_SIZE;
		
	if(!isLargeFile)
	{	
		if(logicalBlockNumber < len(fileInode.addr))
		{
			fileInode.addr[logicalBlockNumber] = blockNumber;
			lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
			write(fd,&fileInode,sizeof(fileInode));
		}
		else
		{
			makeLargefile(inode_number);
			isLargeFile = 1;
		}
	}
		
	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	read(fd,&fileInode,sizeof(fileInode));
	
	if(isLargeFile)
	{
		
		int singleIndirectionblockNumber = (logicalBlockNumber/NUMBER_OF_BLOCKS_PER_INDIRECTION);
		
		DEBUG_LOG("\n\tLarge file additon, logical block %d",logicalBlockNumber);
		DEBUG_LOG("\n\tSingle Indirection logical block Number: %d",singleIndirectionblockNumber);

		//First ten blocks are single indirection blocks
		if(singleIndirectionblockNumber < len(fileInode.addr) -1)
		{
			
			singleIndirectionblockNumber = singleIndirectionblockNumber%len(fileInode.addr);
			//get single indirect block
			singleIndirectblock_t sib;
			
			if(fileInode.addr[singleIndirectionblockNumber] == 0)
				fileInode.addr[singleIndirectionblockNumber] = get_free_block();
			
			DEBUG_LOG("\n\tSingle Indirection block Number: %d",fileInode.addr[singleIndirectionblockNumber]);
			
			lseek(fd,BLOCK_POSITION(fileInode.addr[singleIndirectionblockNumber]),SEEK_SET);
			read(fd,&sib,sizeof(sib));
			
			sib.blockNumbers[logicalBlockNumber%NUMBER_OF_BLOCKS_PER_INDIRECTION] = blockNumber;
			DEBUG_LOG("\n\tAdded block %d inside Single Indirection block Number %d to pos %d",blockNumber,fileInode.addr[singleIndirectionblockNumber],logicalBlockNumber%NUMBER_OF_BLOCKS_PER_INDIRECTION);

			lseek(fd,BLOCK_POSITION(fileInode.addr[singleIndirectionblockNumber]),SEEK_SET);
			write(fd,&sib,sizeof(sib));
		}
		else
		{
			//Any logical number greater than 10 will present inside the last position of addr[] in triple indirection
			singleIndirectionblockNumber = len(fileInode.addr) -1;
			DEBUG_LOG("\n\tVery Large file additon, logical block %d",logicalBlockNumber);
			
			//First level indirection
			//Intialize block numbers inside the indirection
			if(fileInode.addr[singleIndirectionblockNumber] == 0)
				{
					fileInode.addr[singleIndirectionblockNumber] = get_free_block();
					lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
					write(fd,&fileInode,sizeof(fileInode));
				
					singleIndirectblock_t sib;
					for(i=0;i<len(sib.blockNumbers);i++)
						sib.blockNumbers[i] = 0;
					
					lseek(fd,BLOCK_POSITION(fileInode.addr[len(fileInode.addr)-1]),SEEK_SET);
					write(fd,&sib,sizeof(sib));

				}
			
			DEBUG_LOG("\n\tTriple Indirection block Number: %d",fileInode.addr[singleIndirectionblockNumber]);
			
			singleIndirectblock_t sib1;
			lseek(fd,BLOCK_POSITION(fileInode.addr[len(fileInode.addr)-1]),SEEK_SET);
			read(fd,&sib1,sizeof(sib1));
 
			int remainingBlocks = logicalBlockNumber - (NUMBER_OF_BLOCKS_PER_INDIRECTION * (len(fileInode.addr)-1));
			DEBUG_LOG("\n\tRemaining blocks  %d",remainingBlocks);

			
			
			int tripleIndirectionLogicalBlockNumber = (remainingBlocks/(NUMBER_OF_BLOCKS_PER_INDIRECTION
																	*NUMBER_OF_BLOCKS_PER_INDIRECTION));
			if(tripleIndirectionLogicalBlockNumber > len(sib1.blockNumbers))
			{
					printf("MAX File size reached!");
					return;
					
			}
			//second level indirection

			//Intialize block numbers inside the indirection
			if(sib1.blockNumbers[tripleIndirectionLogicalBlockNumber]==0)
			{
				sib1.blockNumbers[tripleIndirectionLogicalBlockNumber] = get_free_block();
				
				singleIndirectblock_t sib;
				for(i=0;i<len(sib.blockNumbers);i++)
					sib.blockNumbers[i] = 0;
					
				lseek(fd,BLOCK_POSITION(sib1.blockNumbers[tripleIndirectionLogicalBlockNumber]),SEEK_SET);
				write(fd,&sib,sizeof(sib));
			}
			
			DEBUG_LOG("\n\tDouble Indirection block Number: %d",sib1.blockNumbers[tripleIndirectionLogicalBlockNumber]);
			
			int tripleIndirectionBlockNumber = sib1.blockNumbers[tripleIndirectionLogicalBlockNumber];


			singleIndirectblock_t sib2;
			lseek(fd,BLOCK_POSITION(tripleIndirectionBlockNumber),SEEK_SET);
			read(fd,&sib2,sizeof(sib2));


			
			int doubleIndirectionLogicalBlockNumber = (remainingBlocks/NUMBER_OF_BLOCKS_PER_INDIRECTION);
			DEBUG_LOG("\n\t Double Indirection logical block Number: %d",doubleIndirectionLogicalBlockNumber);
			

			//third level indirection

			//Intialize block numbers inside the indirection
			if(sib2.blockNumbers[doubleIndirectionLogicalBlockNumber]==0)
			{
				sib2.blockNumbers[doubleIndirectionLogicalBlockNumber] = get_free_block();
				
				singleIndirectblock_t sib;
				
				for(i=0;i<len(sib.blockNumbers);i++)
					sib.blockNumbers[i] = 0;
					
				lseek(fd,BLOCK_POSITION(sib2.blockNumbers[doubleIndirectionLogicalBlockNumber]),SEEK_SET);
				write(fd,&sib,sizeof(sib));

			}
		
			int doubleIndirectionBlockNumber = sib2.blockNumbers[doubleIndirectionLogicalBlockNumber];
			DEBUG_LOG("\n\tSingle Indirection block Number: %d",doubleIndirectionBlockNumber);
			
			singleIndirectblock_t sib3;
			lseek(fd,BLOCK_POSITION(doubleIndirectionBlockNumber),SEEK_SET);
			read(fd,&sib3,sizeof(sib3));

			
			int singleIndirectionBlockNumber = (remainingBlocks%NUMBER_OF_BLOCKS_PER_INDIRECTION);

			if(sib3.blockNumbers[singleIndirectionBlockNumber]==0)
				sib3.blockNumbers[singleIndirectionBlockNumber] = blockNumber;
		
			DEBUG_LOG("\n\tAdded %d to block Number: %d at position %d",blockNumber,doubleIndirectionBlockNumber,singleIndirectionBlockNumber);
			
			lseek(fd,BLOCK_POSITION(doubleIndirectionBlockNumber),SEEK_SET);
			write(fd,&sib3,sizeof(sib3));

			lseek(fd,BLOCK_POSITION(tripleIndirectionBlockNumber),SEEK_SET);
			write(fd,&sib2,sizeof(sib2));

			lseek(fd,BLOCK_POSITION(fileInode.addr[len(fileInode.addr) - 1]),SEEK_SET);
			write(fd,&sib1,sizeof(sib1));
 
		}
	}

	lseek(fd,INODE_POSITION(inode_number),SEEK_SET);
	write(fd,&fileInode,sizeof(fileInode));

}


// Test function to print remaining i-nodes
void print_free_inode_list()
{
	int i = 0,count =0;
	int freeinodelist[(numberOfInodes)-1];
	i = get_free_inode();
	
	printf("\nList of free inodes:");

	inode_t tempinode;

	while(i!=0){
		printf("\n%d",i);
		freeinodelist[count]=i;
		lseek(fd,INODE_POSITION(i),SEEK_SET);

		read(fd,&tempinode,sizeof(tempinode));
		tempinode.flags = tempinode.flags | (1 << 15); // set allocation
		
		lseek(fd,INODE_POSITION(i),SEEK_SET);
		write(fd,&tempinode,sizeof(tempinode));

		i = get_free_inode();
		count++;
	}

	for(i=0;i<count;i++)
	{		
		add_free_inode(freeinodelist[i]);
	}

	printf("\nTotal number of free inodes: %d",count);
}

// Test function to print remaining data blocks
void print_free_block_list()
{
	int i = 0,count =0;
	int freeDBlockList[sb.fsize];

	printf("\nList of free data Blocks:");

	int d = get_free_block();

	while(d)
	{
		printf("\n%d",d);
		freeDBlockList[count] = d;
		d = get_free_block();
			count++;
	}
	printf("\nTotal number of free Data blocks: %d",count);
	 for(i=0;i<count;i++)
	 {
	 	add_to_free_list(freeDBlockList[i]);
	 }
}

/***********************************************************
 * Searches along the file path from the parent directory for the filename and 
 * 		returns the inode of the file if found, 0 if file does not exist
 * *******************************************************/
int searchFileInFileSystem(char *path, int *parent_inode_number,char *fileName)
{
	char* dirName;
	int i;
	int new_inode =0;
	
	dirName = path;
	dirName = strtok(dirName,"/");
	inode_t currentInode;

	while(dirName)
	{
		if(new_inode = fileExists(dirName,*parent_inode_number)) // If directory already exists
		{
			// Check if it is a directory
			lseek(fd,INODE_POSITION(new_inode),SEEK_SET);
			read(fd,&currentInode,sizeof(currentInode));
			short isDirectory = ((currentInode.flags & (1 << 14)) >> 14); // 2nd bit
			
			if(isDirectory)
			{
				*parent_inode_number = new_inode; // Change the parent directory
			}
		}
		else
		{
			if(strcmp(dirName,fileName) == 0)
			{
				return 0; // File does not exist
			}
			else
				return -1; // the middle directory path does not exist
		}

		dirName = strtok(NULL,"/"); // split on '/'
	}

	return new_inode;
}
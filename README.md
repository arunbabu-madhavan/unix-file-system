UNIX v6 File System

Unix V6 file system has a current limitation of 16MB on file size. Redesigned the file system to remove this limitation. 
This program implmentation will enables users to support files up to 4GB in size.
The modified Unix V6 file system was extended to support the commands listed below: 

List of commands supported:
	1. initfs
	2. load
	3. cpin
	4. cpout	
	5. mkdir	
	6. rm	
	7. ls
	8. q
	


SOURCE CODE FILES:

fsaccess.c - The program will read a series of commands from the user and execute them.

The addr[] array is assigned to int data type of size 11. 

To access a small file: We use single level indirection where each element in the array can refer to address of a data block of size 1024 bytes. 
The total file size that can be referenced is 11 * 1024 = 11KB.
To access a large file: We use the first 10 addresses to access data blocks that are referenced using single-level indirection. So first 10 addr[] values can refer to
10*256*1024 = 2.6MB. We use the last value of addr[] array to access data blocks of a large file using triple-level indiretion. 
It can refer to 256*256*256*1024 = 16GB of data.    

The fields of the superblock struct namely isize, fsize, nfree, free[], ninode, inode are all assigned to int data type(each accomodating 4 bytes). 
The total size of superblock is 1023 bytes. 

directory - Each entry in the directory is of 32 bytes, where the first 4 bytes denotes the i-node number and the remaining 28 bytes will be the filename.

The size0 field is assigned to unsigned short type instead of the char type. So, the size0 and size1 fields together contain 4 bytes(32 bits). So, the maximum file size that can be supported in 2^32 which is 4GB. 



EXECUTION:
	
Compile using:
        cc fsaccess.c -lm -o fsaccess 

Execute using:
	      ./fsaccess

When the program runs, you will see the program waiting for user input commands.


List of commands to test:

(1)	initfs: 
		i) Create a file using touch command in the unix. eg: touch test.data
		ii) initfs will accept three arguments:
			(1) the name of the file we created
			(2) number n1 indicating the total number of blocks in the disk (fsize) and
			(3) number n2 representing the total number of i-nodes in the disk.
			eg: initfs test.data 8000 300
		iii) if DEBUG is enabled in the code, the file system initializing steps and 
			the list of free blocks and inodes are printed on the screen.

(2)	q: saves the changes on test.data and exits from the program.

(3)	cpin:   copies the contents of the external file to the file present in the V6 system.
		cpin will accept 2 arguments:
			(i) the external file name.
			(ii) the name of the file present in the V6 system.

(4)	cpout: copies the contents from the file in the V6 system to the external file.
	       cpout will accept 2 arguments :
		       (i) the name of the file present in the V6 system.
		       (ii)the external file name. 

(5)     mkdir: This command creates a directory in the V6 system and sets the first 2 entries to "." and ".."
		Accepts one argument, which will be the name of the new V6 directory.

(6)     rm   : delete the file, free the i-node, remove the file name from the (parent) directory that has this file and add all data blocks of this file
	       to the free list.
	       Accepts one argument, which will be the name of the file to be deleted.
		





 
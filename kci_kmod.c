
/* Declare what kind of code we want from the header files
   Defining __KERNEL__ and MODULE allows us to access kernel-level 
   code not usually available to userspace programs. */

#undef __KERNEL__
#define __KERNEL__ /* We're part of the kernel */
#undef MODULE
#define MODULE /* Not a permanent part, though. */

#include "kci.h" 

#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <asm/uaccess.h>    /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <asm/current.h> 	/* for current global variable */
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <asm/paravirt.h>

MODULE_LICENSE("GPL"); // GNU Public License v2 or later

// Global variables:
static int global_processID;
static int global_fd;
static int cipher_flag;

static struct dentry *file; // "/sys/kernel/debug/kcikmod/calls"
static struct dentry *subdir; // "/sys/kernel/debug/kcikmod"

//static char Message[BUF_LEN]; /* The message the device will give when asked */

unsigned long **sys_call_table; // sys call table - array of pointers to funcions
unsigned long original_cr0; // original register content (read/write privliges)
asmlinkage long (*ref_read)(int fd, const void* __user buf, size_t count); // pointer to original READ
asmlinkage long (*ref_write)(int fd, const void* __user buf, size_t count); // pointer to original WRITE

// To do list:
// 1. How to get the file descriptor from read\write functions?
// 2. simple read?

// the function finds the sys call - from interceptor
static unsigned long **aquire_sys_call_table(void){

	unsigned long int offset = PAGE_OFFSET;
	unsigned long **sct;

	while (offset < ULLONG_MAX) {
		sct = (unsigned long **)offset;

		if (sct[__NR_close] == (unsigned long *) sys_close) 
			return sct;

		offset += sizeof(void *);
	}
	
	return NULL;
}


static long device_ioctl( //struct inode*  inode,
                  struct file*   file,
                  unsigned int   ioctl_num,/* The number of the ioctl */
                  unsigned long  ioctl_param) /* The parameter to it */{

  // set processID
  if(IOCTL_SET_PID == ioctl_num) {
    	global_processID = ioctl_param;
  }

  // set file descriptor
  else if (IOCTL_SET_FD == ioctl_num){
  		global_fd = ioctl_param;
  }

  // set cipher flag
  else if (IOCTL_CIPHER == ioctl_num){
  		cipher_flag = ioctl_param;
  }

  return SUCCESS;
}


/************** Module Declarations *****************/

/* This structure will hold the functions to be called when a process does something to the device we created */

struct file_operations Fops = {
	.read = read_with encryption,
	.write = write_with_encryption,
    .unlocked_ioctl= device_ioctl, 
};

/* Called when module is loaded. Initialize the module - Register the character device */
static int __init simple_init(void) {
	unsigned int ret_val = 0;  

	// init global variables:
	global_processID = -1;
	global_fd = -1;
	cipher_flag = 0;

    // Register a char device. Get newly assigned major num 
    // Fops = our own file operations struct 

    ret_val = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

    if (ret_val < 0) {
        printk(KERN_ALERT "%s failed with %d\n", "Sorry, registering the kci device ", MAJOR_NUM);
        return -1; 
    }

    // create a private log file:
	subdir = debugfs_create_dir(KCIKMOD, NULL); //  for creating a directory inside the debug filesystem.
	if (IS_ERR(subdir))
		return PTR_ERR(subdir);
	if (!subdir)
		return -ENOENT;

	file = debugfs_create_file(CALLS, S_IRUSR, subdir, NULL, &Fops); //  for creating a file in the debug filesystem.
	if (!file) {
		debugfs_remove_recursive(subdir);
		return -ENOENT;
	}

	// change functions is sys calls table:
	if(!(sys_call_table = aquire_sys_call_table()))
		return -1;

	original_cr0 = read_cr0(); // read original register of table

	write_cr0(original_cr0 & ~0x00010000); // flip the bits - now we can write to the table

	ref_read = (void *)sys_call_table[__NR_read]; // save original read sys call
	sys_call_table[__NR_read] = (unsigned long *)read_with encryption; // new sys call is our function
	ref_write = (void *)sys_call_table[__NR_write]; // save original write sys call
	sys_call_table[__NR_write] = (unsigned long *)write_with encryption; // new sys call is our function

	write_cr0(original_cr0); // write back the original register content

    return SUCCESS;
}


/* a process which has already opened the device file attempts to read from it */


asmlinkage long read_with_encryption(int fd, const void* __user buf, size_t count){
	int bytes_read_from_fd; // original read return value

	bytes_read_from_fd = ref_read(fd, buf, count); // original READ call!

	if ( (cipher_flag == 1) && (current->pid == global_processID) && (global_fd == fd)){ // encrypt!

		for (i = 0; i < bytes_read_from_fd; i++){
			buf[i] += 1;
		}

		// write to the private log file
		pr_debug("read_with_encryption: FD: %d ,PID: %d, number of bytes read: %d\n",global_fd ,global_processID, bytes_read_from_fd); 

	}

	// return value - like original read call 
	return bytes_read_from_fd;

}

//static ssize_t device_read(struct file *file, /* see include/linux/fs.h   */
//               char __user * buffer, /* buffer to be filled with data */
 //              size_t length,  /* length of the buffer     */
 //              loff_t * offset){

  /* 
   	int i;
   	int fd_to_read_from = 0;

	if ( (cipher_flag == 1) && (current->pid == global_processID) && (//compare fd ) ){ // encrypt!

		for (i = 0; i < length && i < BUF_LEN; i++){
			get_user(Message[i], buffer + i); // read
			Message[i] += 1; // dectyped
			put_user(Message[i], buffer + i); // return to user
		}
		pr_debug("device_read: FD: %d ,PID: %d, number of bytes read: %d\n",global_fd ,global_processID, i); // writes to the private log file?

	}

	else{ // dont encrypt!

		fd_to_read_from = ;

		i = read(// file descriptor //, buffer,length);


	}
	
	
 
	// return the number of input characters used 
	return i;
}
*/


/* somebody tries to write into our device file */


asmlinkage long write_with_encryption(int fd, const void* __user buf, size_t count){
	int bytes_written_to_fd; // original read return value

	bytes_written_to_fd = ref_write(fd, buf, count); // original WRITE call!

	if ( (cipher_flag == 1) && (current->pid == global_processID) && (global_fd == fd)){ // encrypt!

		for (i = 0; i < bytes_written_to_fd; i++){
			buf[i] += 1;
		}

		// write to the private log file
		pr_debug("write_with_encryption: FD: %d ,PID: %d, number of bytes aritten: %d\n",global_fd ,global_processID, bytes_written_to_fd); 

	}

	// return value - like original write call 
	return bytes_written_to_fd;

}


/*static ssize_t device_write(struct file *file,
         					const char __user * buffer, size_t length, loff_t * offset) {

	int i;

	if ( (cipher_flag == 1) && (current->pid == global_processID) && ()){ // encrypt!

		for (i = 0; i < length && i < BUF_LEN; i++){
			get_user(Message[i], buffer + i);
			Message[i] += 1;
		}
		pr_debug("device_write: FD: %d ,PID: %d, number of bytes written: %d\n",global_fd ,global_processID, i); // writes to the private log file?

	}

	else{ // dont encrypt!

		i = write(,buffer,length);

	}
	
	
 
	// return the number of input characters used 
	return i;
}*/

/* Cleanup - unregister the appropriate file from /proc */
static void __exit simple_cleanup(void){
    /*  Unregister the device should always succeed (didnt used to in older kernel versions) */

    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME); // return to original system calls
    debugfs_remove_recursive(subdir); // clears log

    // restore original sys calls
    if(!sys_call_table) {
		return;
	}

	write_cr0(original_cr0 & ~0x00010000); // flip the bits - now we can write to the table
	sys_call_table[__NR_read] = (unsigned long *)ref_read; // restore READ
	sys_call_table[__NR_write] = (unsigned long *)ref_write; // restore WRITE
	write_cr0(original_cr0); // the register gets its original content
	
	msleep(2000);
}

module_init(simple_init);
module_exit(simple_cleanup);

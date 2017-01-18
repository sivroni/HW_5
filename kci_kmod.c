
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

MODULE_LICENSE("GPL");

// Global variables:
static int processID;
static int fd;
static int cipher_flag;

static struct dentry *file; // "/sys/kernel/debug/kcikmod/calls"
static struct dentry *subdir; // "/sys/kernel/debug/kcikmod"

static char Message[BUF_LEN]; /* The message the device will give when asked */

// To do list:
// 1.


static long device_ioctl( //struct inode*  inode,
                  struct file*   file,
                  unsigned int   ioctl_num,/* The number of the ioctl */
                  unsigned long  ioctl_param) /* The parameter to it */{

  // set processID
  if(IOCTL_SET_PID == ioctl_num) {
    	processID = ioctl_param;
  }

  // set file descriptor
  else if (IOCTL_SET_FD == ioctl_num){
  		fd = ioctl_param;
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
    .read = device_read,
    .write = device_write,
    .unlocked_ioctl= device_ioctl,
    // .open = device_open,
    // .release = device_release,  
};

/* Called when module is loaded. Initialize the module - Register the character device */
static int __init simple_init(void) {
	unsigned int ret_val = 0;  

	// init global variables:
	processID = -1;
	fd = -1;
	cipher_flag = 0;

    // Register a char device. Get newly assigned major num 
    // Fops = /* our own file operations struct */

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



    return 0;
}


/* a process which has already opened the device file attempts to read from it */
static ssize_t device_read(struct file *file, /* see include/linux/fs.h   */
               char __user * buffer, /* buffer to be filled with data */
               size_t length,  /* length of the buffer     */
               loff_t * offset){

   
   	int i;

	if ( (cipher_flag == 1) && (/*compare id and fd */) ){ // encrypt!

		for (i = 0; i < length && i < BUF_LEN; i++){
			get_user(Message[i], buffer + i); // read
			Message[i] += 1; // dectyped
			put_user(Message[i], buffer + i); // return to user
		}
		printk("device_read: FD: %d ,PID: %d, number of bytes read: %d\n",fd ,processID, i); // writes to the private log file?

	}

	else{ // dont encrypt!

		for (i = 0; i < length && i < BUF_LEN; i++){
			get_user(Message[i], buffer + i);
		}

	}
	
	
 
	/* return the number of input characters used */
	return i;
}

/* somebody tries to write into our device file */
static ssize_t device_write(struct file *file,
         					const char __user * buffer, size_t length, loff_t * offset) {

	int i;

	if ( (cipher_flag == 1) && (/*compare id and fd */) ){ // encrypt!

		for (i = 0; i < length && i < BUF_LEN; i++){
			get_user(Message[i], buffer + i);
			Message[i] += 1;
		}
		printk("device_write: FD: %d ,PID: %d, number of bytes written: %d\n",fd ,processID, i); // writes to the private log file?

	}

	else{ // dont encrypt!

		for (i = 0; i < length && i < BUF_LEN; i++){
			get_user(Message[i], buffer + i);
		}

	}
	
	
 
	/* return the number of input characters used */
	return i;
}


module_init(simple_init);
module_exit(simple_cleanup);

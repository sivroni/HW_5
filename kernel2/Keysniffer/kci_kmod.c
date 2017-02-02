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
#include <linux/init.h>
#include <linux/debugfs.h>


#define BUF_LEN (PAGE_SIZE << 2) // 16KB buffer (assuming 4KB PAGE_SIZE) - for keys_buf size

MODULE_LICENSE("GPL"); // GNU Public License v2 or later

// Global variables:
static int global_processID = -1;
static int global_fd = -1;
static int cipher_flag = 0;

static struct dentry *file; // basically "/sys/kernel/debug/kcikmod/calls"
static struct dentry *subdir; // basically "/sys/kernel/debug/kcikmod"

static size_t buf_pos; // for keys_read
static char keys_buf[BUF_LEN] = {0}; // for keys_read

unsigned long **sys_call_table; // sys call table - array of pointers to funcions
unsigned long original_cr0; // original register content (read/write privliges)
asmlinkage long (*ref_read)(int fd, const void* __user buf, size_t count); // pointer to original READ
asmlinkage long (*ref_write)(int fd, const void* __user buf, size_t count); // pointer to original WRITE

// To do list:
// 1. Comment 6 - what is missing?

// Headers:
static ssize_t keys_read(struct file *filp, char *buffer, size_t len, loff_t *offset); // header for key_read for debugfs
asmlinkage long read_with_encryption(int fd, const void* __user buf, size_t count); // new read function
asmlinkage long write_with_encryption(int fd, const void* __user buf, size_t count); // new write function



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


/* Holds the debugfs operations */
const struct file_operations Fops_debug = {
	.owner = THIS_MODULE,
	.read = keys_read, 
};

/* This structure will hold the functions to be called when a process does something to the device we created */
const struct file_operations Fops_chrdev = {
    .unlocked_ioctl= device_ioctl, 
};

static ssize_t keys_read(struct file *filp, char *buffer, size_t len, loff_t *offset){
	return simple_read_from_buffer(buffer, len, offset, keys_buf, buf_pos);
}

/* Called when module is loaded. Initialize the module - Register the character device */
static int __init simple_init(void) {
	unsigned int ret_val = 0;  

	// init global variables:
	global_processID = -1;
	global_fd = -1;
	cipher_flag = 0;

	// create a private log file:
	subdir = debugfs_create_dir(KCIKMOD, NULL); //  for creating a directory inside the debug filesystem.
	if (IS_ERR(subdir))
		return PTR_ERR(subdir);
	if (!subdir)
		return -ENOENT;

	file = debugfs_create_file(CALLS, S_IRUSR, subdir, NULL, &Fops_debug); //  for creating a file in the debug filesystem.
	if (!file) {
		debugfs_remove_recursive(subdir);
		return -ENOENT;
	}

    // Register a char device. Get newly assigned major num 
    // Fops = our own file operations struct 

    ret_val = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops_chrdev );

    if (ret_val < 0) {
        printk(KERN_ALERT "%s failed with %d\n", "Sorry, registering the kci device ", MAJOR_NUM);
        return -1; 
    }

    buf_pos = 0; // init buffer for debugfs
 
	// change functions is sys calls table:
	if(!(sys_call_table = aquire_sys_call_table()))
		return -1;
 
	original_cr0 = read_cr0(); // read original register of table

	write_cr0(original_cr0 & ~0x00010000); // flip the bits - now we can write to the table

	ref_read = (void *)sys_call_table[__NR_read]; // save original read sys call
	sys_call_table[__NR_read] = (unsigned long *)read_with_encryption; // new sys call is our function
	ref_write = (void *)sys_call_table[__NR_write]; // save original write sys call
	sys_call_table[__NR_write] = (unsigned long *)write_with_encryption; // new sys call is our function

	write_cr0(original_cr0); // write back the original register content

    return SUCCESS;
}


/* a process which has already opened the device file attempts to read from it */


asmlinkage long read_with_encryption(int fd, const void* __user buf, size_t count){
	int bytes_read_from_fd = 0; // original read return value
	int i = 0;
	char value;
	size_t len; // for prdebug
	char str[MAX]; // for debugfs

	bytes_read_from_fd = ref_read(fd, buf, count); // original READ call!
	if (bytes_read_from_fd < 0){
		 printk(KERN_ALERT "%s failed with\n", "Faild reading with original call ");
        return -1; 
	}

	if ( (cipher_flag == 1) && (current->pid == global_processID) && (global_fd == fd)){ // decrypt!

		for (i = 0; i < bytes_read_from_fd; i++){
			//value = *((char *)buf + i) -1;
			get_user(value,((char *)buf + i) ); // get value from buffer
			value = value -1; // decrypt value
			put_user(value, ((char *)buf + i)); // update buffer
		}

		// write to the private log file
		if (sprintf(str, "read_with_encryption: FD: %d ,PID: %d, bytes read: %d\n",global_fd ,global_processID, bytes_read_from_fd) < 0){
			return -1;	
		}

		len = strlen(str);

		if ((buf_pos + len) >= BUF_LEN) {
			memset(keys_buf, 0, BUF_LEN);
			buf_pos = 0;
		}

		strncpy(keys_buf + buf_pos, str, len);
		buf_pos += len;
		//keys_buf[buf_pos++] = '\n';

		pr_debug("%s\n", str);	
			 
		// change to sniffer_cb where pressed_key need to be the msg (save the "if" in the function)
	}

	// return value - like original read call 
	return bytes_read_from_fd;

}


/* somebody tries to write into our device file */


asmlinkage long write_with_encryption(int fd, const void* __user buf, size_t count){
	int bytes_written_to_fd = 0; // original write return value
	int i = 0;
	int value = 0;
	size_t len; // for prdebug
	char str[MAX]; // for debugfs

	if ( (cipher_flag == 1) && (current->pid == global_processID) && (global_fd == fd)){ // encrypt!

		for (i = 0; i < count; i++){
			//value = *((char *)buf + i) +1;
			get_user(value, ((char *)buf + i)); // get value from buffer
			value = value + 1; // encrypt value
			put_user(value, ((char *)buf + i)); // update buffer with encrypted data
		}		 

	}

	bytes_written_to_fd = ref_write(fd, buf, count); // original WRITE call!
	if (bytes_written_to_fd < 0){
		 printk(KERN_ALERT "%s failed with\n", "Faild writing with original call ");
        return -1; 
	}

	// updated buffer back

	if ( (cipher_flag == 1) && (current->pid == global_processID) && (global_fd == fd)){ // encrypt!

		for (i = 0; i < count; i++){
			//value = *((char *)buf + i) -1;
			get_user(value, ((char *)buf + i)); // get value from buffer
			value = value - 1; // encrypt value
			put_user(value, ((char *)buf + i));
		}

		// write to the private log file
		if (sprintf(str, "write_with_encryption: FD: %d ,PID: %d, bytes wrriten: %d\n",global_fd ,global_processID, bytes_written_to_fd) < 0){
			return -1;	
		}

		len = strlen(str);

		if ((buf_pos + len) >= BUF_LEN) {
			memset(keys_buf, 0, BUF_LEN);
			buf_pos = 0;
		}

		strncpy(keys_buf + buf_pos, str, len);
		buf_pos += len;
	//	keys_buf[buf_pos++] = '\n';

		pr_debug("%s\n", str);	
			 
	}

	// return value - like original write call 
	return bytes_written_to_fd;

}


/* Cleanup - unregister the appropriate file from /proc */
static void __exit simple_cleanup(void){
    /*  Unregister the device should always succeed (didnt used to in older kernel versions) */

    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME); // return to original system calls
    // how to check errors here?

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

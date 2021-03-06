#include "kci.h"    

#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h> 
#include <errno.h>

///// define commands: /////
#define INIT_STR "-init"
#define PID_STR "-pid"
#define FD_STR "-fd"
#define START_STR "-start"
#define STOP_STR "-stop"
#define RM_STR "-rm"
////////////////////////////

#define LOF_FILE_PATH "/sys/kernel/debug/kcikmod/calls"

#define NUM_OF_BYTES 1024 // for buffer

// Headers:

void copyLog();

const char *KCI_NAME = "kci_kmod";


void copyLog(){

	int inner_calls_fd = 0;
	int log_file_fd = 0;
	int bytes_read_from_log = 0;
	int total_bytes_written_to_calls = 0;
	int bytes_written_to_calls = 0;
	char logBuffer[NUM_OF_BYTES] = {0}; // insert bytes read from log file here

	// create "calls file"
	inner_calls_fd = open(CALLS, O_WRONLY | O_CREAT | O_TRUNC,0777 ); // opens/creates an output file 
	if (inner_calls_fd < 0){ // check for error 
			printf("Error creating 'calls' file in current directory: %s\n", strerror(errno));
			exit(errno); 
	}

	// open log file for reading 
   	log_file_fd = open(LOF_FILE_PATH, O_RDONLY); // open IN file
	
   	if( log_file_fd < 0 ){
        	printf("Error opening log file : %s\n", strerror(errno) );
        	exit(errno); 
	}

	 // copy the data from log file to the calls in current directory

	while (1){ // read until we have nothing to read from log file

			bytes_read_from_log = read(log_file_fd, logBuffer, NUM_OF_BYTES); // try reading from IN

			if (bytes_read_from_log < 0){ // error reading from client
				printf("Error reading from log file: %s\n", strerror(errno));
				exit(errno); 
			}

			else if (bytes_read_from_log == 0){ // finish reading - THIS WILL END THE WHILE LOOP
				break;
			}
			
			total_bytes_written_to_calls = 0; // sum the bytes we write - make sure we wrote everything
			while (total_bytes_written_to_calls < bytes_read_from_log) {
				
				bytes_written_to_calls = write(inner_calls_fd, logBuffer + total_bytes_written_to_calls, bytes_read_from_log - total_bytes_written_to_calls);
				if (bytes_written_to_calls < 0) {
					printf("error write() to 'calls' file : %s\n", strerror(errno));
					exit(errno);
				}

				// increment our counter
				else{
					total_bytes_written_to_calls = total_bytes_written_to_calls + bytes_written_to_calls;
				}

			} // finished writing to inner calls file the current buffer
		} 
		// noting else to read from log file - finished the loop

		// close files when done 
		close(log_file_fd);
		close(inner_calls_fd); 


}

int main(int argc, char *argv[]){

	// argv[1] == command, argv[2] == argument, if exists 

	int device_file_desc; // file descriptor for /dev/kci_dev
	int ret_val; // return value for several functions
	char * ko_path; // .ko path to our compiled file
	dev_t dev; // makedev return value, A device ID is represented using the type dev_t.
	long PID; // process id for set_pid
	long FD; // argument for set_fd
	char * ptr; // for strtoul function
	int i=1; // our command is at argv[1]
	int ko_fd = 0;

	if (argc < 2){  // we need at least one <command>
		printf("Not enough arguments were entered...\n");
		exit(-1);
	}


	if (strcmp(argv[i], INIT_STR) == 0){ // INIT COMMAND

			if (argc  != 3){ // check number of arguments
				printf("Not enough arguments were entered for INIT command...\n");
				exit(-1);
			}


			dev =  makedev(MAJOR_NUM, 0); // produces a device ID , WHAT ABOUT ERRORS?

			ret_val =  mknod(DEVICE_FILE_PATH, S_IFCHR , dev); // CHECK flags
			if (ret_val < 0){
				printf("Error: can't mknod : %s\n", strerror(errno));
				close(ko_fd);
				unlink(KCI_NAME);
				exit(errno);
			}
		
			ko_path = argv[i+1];
		
			ko_fd = open(ko_path, O_RDONLY); // create a file descriptor for .ko file
			if (ko_fd < 0) {
			    printf ("Can't open ko file: %s\n", strerror(errno));
			    exit(errno);
			}
		
			ret_val = syscall(__NR_finit_module, ko_fd, "",0); // load module from ko_path 
			if (ret_val < 0){
				printf("Error: can't load ko file : %s\n", strerror(errno));
				close(ko_fd);
				exit(errno);
			}

			close(ko_fd);


	} // END OF INIT COMMAND

	else if (strcmp(argv[i], PID_STR) == 0){ // PID COMMAND

			if (argc != 3){ // check number of arguments
				printf("Not enough arguments were entered for PID command...\n");
				exit(-1);
			}

			// convert pid to int
			errno = 0;
			PID = strtol(argv[i+1], &ptr, 10); 
			if (errno != 0){
					printf("Error converting PID from string to unsigned long: %s\n", strerror(errno));
					exit(errno);
			}

			device_file_desc = open(DEVICE_FILE_PATH, 0); // create a file descriptor for /dev/kci_dev
			if (device_file_desc < 0) {
			    printf ("Can't open device file: %s\n", strerror(errno));
			    exit(errno);
			}

			ret_val = ioctl(device_file_desc, IOCTL_SET_PID, PID);
			if (ret_val < 0){
				printf("Error in ioctl - IOCTL_SET_PID : %s\n", strerror(errno));
				exit(errno);
			}

			close(device_file_desc);

		} // END OF PID COMMAND

		else if(strcmp(argv[i], FD_STR) == 0){ // FD COMMAND
			// open a file separately and then send the fd to terminal for this command

			if (argc != 3){ // check number of arguments
				printf("Not enough arguments were entered for FD command...\n");
				exit(-1);
			}

			errno = 0;
			FD = strtol(argv[i+1], &ptr, 10); 
			if (errno != 0){
					printf("Error converting FD from string to unsigned long: %s\n", strerror(errno));
					exit(errno);
			}

			device_file_desc = open(DEVICE_FILE_PATH, 0); // create a file descriptor for /dev/kci_dev
			if (device_file_desc < 0) {
			    printf ("Can't open device file: %s\n", strerror(errno));
			    exit(errno);
			}

			ret_val = ioctl(device_file_desc, IOCTL_SET_FD, FD);
			if (ret_val < 0){
				printf("Error in IOCTL_SET_PID : %s\n", strerror(errno));
				exit(errno);
			}

			close(device_file_desc);

		} // END OF FD COMMAND

		else if(strcmp(argv[i], START_STR) == 0){ // CIPHER ENCRYPT COMMAND

			device_file_desc = open(DEVICE_FILE_PATH, 0); // create a file descriptor for /dev/kci_dev
			if (device_file_desc < 0) {
			    printf ("Can't open device file: %s\n", strerror(errno));
			    exit(errno);
			}

			ret_val = ioctl(device_file_desc, IOCTL_CIPHER, 1);
			if (ret_val < 0){
				printf("Error stating encryption : %s\n", strerror(errno));
				exit(errno);
			}

			close(device_file_desc);

		} // END OF CIPHER ENCRYPT COMMAND

		else if(strcmp(argv[i], STOP_STR) == 0){ // CIPHER UN-ENCRYPT COMMAND

			device_file_desc = open(DEVICE_FILE_PATH, 0); // create a file descriptor for /dev/kci_dev
			if (device_file_desc < 0) {
			    printf ("Can't open device file: %s\n", strerror(errno));
			    exit(errno);
			}

			ret_val = ioctl(device_file_desc, IOCTL_CIPHER, 0);
			if (ret_val < 0){
				printf("Error stopping encryption : %s\n", strerror(errno));
				exit(errno);
			}
			close(device_file_desc);


		} // END OF CIPHER UN-ENCRYPT COMMAND

		else if(strcmp(argv[i], RM_STR) == 0){
			// copy log file to inner 'calls' file
			copyLog();

			// remove kernel module - delete_module
			ret_val = syscall(__NR_delete_module, DEVICE_RANGE_NAME,  O_NONBLOCK); // check flags
			if (ret_val < 0){
				printf("Error in delet_module syscall : %s\n", strerror(errno));
				exit(errno);
			}

			if( unlink(DEVICE_FILE_PATH) < 0){ // remove the mknod
				printf("Error in unlink : %s\n", strerror(errno));
				exit(errno);
			} 
			

		}
		else { // wrong command
			printf("Wrong command entered, closing program...\n");
			exit(-1);
		}


	exit(0);

}
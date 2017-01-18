
#include "kci.h"    

#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

// Headers:

void copyLog();

// To do list:
// 1. Is each flow size 6 ?

struct current_struct{
	pid_t pid; // current process id
};

struct current_struct current; 
const char *DEVICE_FILE_PATH = "/dev/kci_dev"; // device file path


void copyLog(){

	int inner_calls_fd = 0;
	int log_file_fd = 0;
	int bytes_read_from_log = 0;
	int total_bytes_written_to_calls = 0;
	int bytes_written_to_calls = 0;
	char logBuffer[MAX] = {0}; // insert bytes read from log file here

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

			bytes_read_from_log = read(log_file_fd, logBuffer, MAX); // try reading from IN

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

			} // finished writing to server the current buffer
		}

		// close file when done 
		close(log_file_fd);
		close(inner_calls_fd); 


}

int main(int argc, char *argv[]){

	// argv[1] == init, argv[2] == argument ko, ..... , arg[argc-1] == rm 

	int file_desc; // file descriptor for /dev/kci_dev
	int ret_val; // return value for several functions
	char * ko_path;
	dev_t dev; // makedev return value, A device ID is represented using the type dev_t.
	unsigned long PID; // process id for set_pid
	int FD; // argument for set_fd
	char * ptr; // for strtoul function

	if (argc < 3){
		printf("Not enough arguments entered\n");
		exit(-1);
	}

	for (i=1; i<argc; i++){

		if (strcmp(argv[i], INIT_STR) == 0){ // INIT COMMANDS

			ko_path = argv[i+1];

			ret_val = syscall(__NR_finit_module, ko_path, "",0); // load module from ko_path 
			if (ret_val < 0){
				printf("Error: can't load ko file : %s\n", strerror(errno));
				exit(errno);
			}

			dev =  makedev(MAJOR_NUM, 0); // produces a device ID , WHAT ABOUT ERRORS?

			ret_val =  mknod(DEVICE_FILE_PATH, S_IFREG | 0777 , dev); // CHECK
			if (ret_val < 0){
				printf("Error: can't mknod : %s\n", strerror(errno));
				exit(errno);
			}
			i++; // we actually read 2 arguments - init and ko_path
			cotinue; // go to next iteration


		} // END OF INIT COMMAND

		else if (strcmp(argv[i], PID_STR) == 0){ // PID COMMAND

			errno = 0;
			PID = strtoul(argv[i+1], &ptr, 10); // maybe getpid() instead?
			if (errno != 0){
					printf("Error converting PID from string to unsigned long: %s\n", strerror(errno));
					exit(errno);
			}

			ret_val = ioctl(file_desc, 	IOCTL_SET_PID, PID);
			if (ret_val < 0){
				printf("Error in IOCTL_SET_PID : %s\n", strerror(errno));
				exit(errno);
			}

			i++;
			continue;

		} // END OF PID COMMAND

		else if(strcmp(argv[i], FD_STR) == 0){ // FD COMMAND
			errno = 0;
			FD = strtol(argv[i+1], &ptr, 10); // ?????
			if (errno != 0){
					printf("Error converting FD from string to unsigned long: %s\n", strerror(errno));
					exit(errno);
			}

			ret_val = ioctl(file_desc, 	IOCTL_SET_FD, FD);
			if (ret_val < 0){
				printf("Error in IOCTL_SET_PID : %s\n", strerror(errno));
				exit(errno);
			}

			i++;
			continue;

		} // END OF FD COMMAND

		else if(strcmp(argv[i], START_STR) == 0){ // CIPHER ENCRYPT COMMAND

			ret_val = ioctl(file_desc, 	IOCTL_CIPHER, 1);
			if (ret_val < 0){
				printf("Error stating encryption : %s\n", strerror(errno));
				exit(errno);
			}

			continue;

		} // END OF CIPHER ENCRYPT COMMAND

		else if(strcmp(argv[i], STOP_STR) == 0){ // CIPHER UN-ENCRYPT COMMAND

			ret_val = ioctl(file_desc, 	IOCTL_CIPHER, 0);
			if (ret_val < 0){
				printf("Error stopping encryption : %s\n", strerror(errno));
				exit(errno);
			}

			continue;

		} // END OF CIPHER UN-ENCRYPT COMMAND

		else if(strcmp(argv[i], RM_STR) == 0){
			// copy log file to inner 'calls' file
			copyLog();

			// remove kernel module - delete_module
			ret_val = syscall(__NR_delete_module, ko_path,  O_NONBLOCK); // check flags
			if (ret_val < 0){
				printf("Error inc delet_module syscall : %s\n", strerror(errno));
				exit(errno);
			}
			// remove device file created by init

		}

	} // END OF FOR LOOP

	exit(0);

}
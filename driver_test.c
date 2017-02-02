#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>



#define ARGS_TOTAL_NUMBER (5)

#define ARG_INDEX_OF_CTRL (1)
#define ARG_INDEX_OF_KO (2)
#define ARG_INDEX_OF_FILE_CHECK (3)
#define ARG_INDEX_OF_EXTRA_TEST (4)


#define CREATE_OVERRIDE_FILE_FOR_RW_FALGS (O_CREAT | O_RDWR | O_TRUNC)

#define OPEN_FILE_FOR_READING_FALGS (O_RDONLY)

#define READ_FAILURE		(-1)
#define WRITE_FAILURE		(-1)

#define EXIT_OS_FAILURE		(-1)
#define EXIT_OS_SUCCESS		(0)


#define BUFFER_SIZE (10) /* read file max size */


#define MAX_CTRL_MESSSAGE (200)

#define LOG_FILE_NAME 	"calls"

#define INIT "./%s -init %s"
#define PID "./%s -pid %d"
#define FD "./%s -fd %d"
#define START "./%s -start"
#define STOP "./%s -stop"
#define REMOVE "./%s -rm"

#define STATUS_SHELL ("echo \"lsmod:\";" \
"lsmod | grep kci;" \
"echo \"device file:\" ;" \
"ls /dev/ | grep kci;" \
"echo \"debugfs folder:\";" \
"ls /sys/kernel/debug | grep kci;" \
"echo ")



#define DATA_INPUT_1	("abcdefghij")
#define DATA_INPUT_2	("ABCDEFGHIJ")

#define DATA_INPUT_ENCRYPT_1 ("bcdefghijk")
#define DATA_INPUT_ENCRYPT_2 ("BCDEFGHIJK")

/****************
 * Declarations *
 ****************/

/* run command *ctrlSystemCommand* and check for errors */
int runSyscall(char* ctrlSystemCommand, int lineNumber);

/* write *dataInputBuf* of size *bytesWriteCount* to file
 * 		+ check that dataInputBuf was not change after calling write  */
int writeLoop(int fd, char* dataInputBuf, int bytesWriteCount);

/* read data from file to dataInputBuf of size *bytesReadCount* */
int readLoop(int fd, char* dataInputBuf, int bytesReadCount);

/* compare buffer of size *size* */
int checkSameData(char* bufOrg, char* bufRead, int size);



/******
 * All the test are here (expect for init and remove which are on main)
 ***/
int mainLogic(int fd, char* ctrlPath, int isExtraTest)
{
	int ret;
	char dataRead[BUFFER_SIZE];
	char dataWrite[BUFFER_SIZE];

	char ctrlSystemCommand[MAX_CTRL_MESSSAGE];

	int currentPid = 0;


	printf("Test: setting pid and fd\n");

	currentPid = getpid();
	/* call pid */
	memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
	sprintf(ctrlSystemCommand, PID, ctrlPath, currentPid);

	ret = runSyscall(ctrlSystemCommand, __LINE__);
	if (0 != ret)
	{
		return ret;
	}

	printf("Good: Set pid icotl worked\n");

	/* call fd */
	memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
	sprintf(ctrlSystemCommand, FD, ctrlPath, fd);

	ret = runSyscall(ctrlSystemCommand, __LINE__);
	if (0 != ret)
	{
		return ret;
	}

	printf("Good: Set fd icotl worked\n\n");


	/*****
	 * Check regFlow - without cipher flag on
	 */

	printf("Test: Check encryption when encryption flag is off "
			"(this is default state on init driver)\n");

	memcpy(dataWrite, DATA_INPUT_1, BUFFER_SIZE);
	if (0 != writeLoop(fd, dataWrite, BUFFER_SIZE))
	{
		return -1;
	}

	/* Reset the file to start -> override last written data*/
	if (-1 == lseek(fd, 0, SEEK_SET))
	{
		 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
				 "error: %s, Line:%d\n", strerror(errno), __LINE__);

		 return -1;
	}

	if (0 != readLoop(fd, dataRead, BUFFER_SIZE))
	{
		return -1;
	}

	if (0 != checkSameData(DATA_INPUT_1, dataRead, BUFFER_SIZE))
	{
		printf ("Bad: Not the same Data! Line:%d\n", __LINE__);
		return -1;
	}

	/***********
	 * SUCCESS *
	 ***********/
	printf("Good: Non encryption is working!\n\n");



	/*****
	 * Check flow - with cipher flag on
	 */
	printf("Test: check write-read when encryption is on (and pid+fd is set right)\n");

	/* call start */
	memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
	sprintf(ctrlSystemCommand, START, ctrlPath);

	ret = runSyscall(ctrlSystemCommand, __LINE__);
	if (0 != ret)
	{
		return ret;
	}


	/* Reset the file to start -> override last written data*/
	if (-1 == lseek(fd, 0, SEEK_SET))
	{
		 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
				 "error: %s, Line:%d\n", strerror(errno), __LINE__);

		 return -1;
	}
	printf("Good: encryption flag was set\n");


	memcpy(dataWrite, DATA_INPUT_1, BUFFER_SIZE);
	if (0 != writeLoop(fd, dataWrite, BUFFER_SIZE))
	{
		return -1;
	}

	/* Reset the file to start */
	if (-1 == lseek(fd, 0, SEEK_SET))
	{
		 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
				 "error: %s, Line:%d\n", strerror(errno), __LINE__);

		 return -1;
	}
	/* read data */
	if (0 != readLoop(fd, dataRead, BUFFER_SIZE))
	{
		return -1;
	}

	if (0 != checkSameData(DATA_INPUT_1, dataRead, BUFFER_SIZE))
	{
		printf ("Bad: Not the same Data! Line:%d\n", __LINE__);
		return -1;
	}

	printf("Good: Great job!! encrypt is working!\n\n");



	/* next test */
	printf("Test: Now we run a deeper check...\n"
			"     We set off the encrypt mode - and actually check the that\n"
			"     the file was encrypted properly\n"
			"     (maybe you just wrote the data to file without encryption ;-])\n");

	/* call stop */
	memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
	sprintf(ctrlSystemCommand, STOP, ctrlPath);

	ret = runSyscall(ctrlSystemCommand, __LINE__);
	if (0 != ret)
	{
		return ret;
	}


	/* Reset the file to start */
	if (-1 == lseek(fd, 0, SEEK_SET))
	{
		 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
				 "error: %s, Line:%d\n", strerror(errno), __LINE__);

		 return -1;
	}

	if (0 != readLoop(fd, dataRead, BUFFER_SIZE))
	{
		return -1;
	}

	if (0 != checkSameData(DATA_INPUT_ENCRYPT_1, dataRead, BUFFER_SIZE))
	{
		printf ("Bad: Got non encrypted data! Line:%d\n", __LINE__);
		return -1;
	}

	printf("Good: The file is really encrypted!!\n\n");


	/****
	 * encrypt is off
	 **/
	printf("Test: Now we will just write and read (encrypt mode is off)\n");

	/* Reset the file to start */
	if (-1 == lseek(fd, 0, SEEK_SET))
	{
		 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
				 "error: %s, Line:%d\n", strerror(errno), __LINE__);

		 return -1;
	}

	/*****
	 * Check regFlow - without cipher flag on
	 */
	memcpy(dataWrite, DATA_INPUT_2, BUFFER_SIZE);
	if (0 != writeLoop(fd, dataWrite, BUFFER_SIZE))
	{
		return -1;
	}

	/* Reset the file to start */
	if (-1 == lseek(fd, 0, SEEK_SET))
	{
		 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
				 "error: %s, Line:%d\n\n", strerror(errno), __LINE__);

		 return -1;
	}

	if (0 != readLoop(fd, dataRead, BUFFER_SIZE))
	{
		return -1;
	}

	if (0 != checkSameData(DATA_INPUT_2, dataRead, BUFFER_SIZE))
	{
		printf ("Bad: Not the same Data as expected! Line:%d\n", __LINE__);
		return -1;
	}

	printf("Good: data was not encrypted\n\n");

	if (1 == isExtraTest)
	{
		printf("\nGreat Job! you got to EXTRA TEST stage:\n");

		/***************
		 * ioctl stuff *
		 ***************/
		printf("    On this stage we will try to play with ioctl parameters -\n"
			   "    to check that we encrypt *only* the right file.\n\n");


		printf("Test: Check that module does not encrypt when pid is wrong.\n");
		/* call start */
		memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
		sprintf(ctrlSystemCommand, START, ctrlPath);

		ret = runSyscall(ctrlSystemCommand, __LINE__);
		if (0 != ret)
		{
			return ret;
		}


		/* Reset the file to start */
		if (-1 == lseek(fd, 0, SEEK_SET))
		{
			 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
					 "error: %s, Line:%d\n\n", strerror(errno), __LINE__);

			 return -1;
		}

		/* call pid - with different pid (currentPid+10000) */
		memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
		sprintf(ctrlSystemCommand, PID, ctrlPath, currentPid+10000);

		ret = runSyscall(ctrlSystemCommand, __LINE__);
		if (0 != ret)
		{
			return ret;
		}


		printf("Good: Set wrong pid icotl worked\n");

		/* check non encryption */
		memcpy(dataWrite, DATA_INPUT_1, BUFFER_SIZE);
		if (0 != writeLoop(fd, dataWrite, BUFFER_SIZE))
		{
			return -1;
		}

		/* Reset the file to start */
		if (-1 == lseek(fd, 0, SEEK_SET))
		{
			 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
					 "error: %s, Line:%d\n\n", strerror(errno), __LINE__);

			 return -1;
		}

		if (0 != readLoop(fd, dataRead, BUFFER_SIZE))
		{
			return -1;
		}

		if (0 != checkSameData(DATA_INPUT_1, dataRead, BUFFER_SIZE))
		{
			printf ("Bad: Not the same Data as expected! Line:%d\n", __LINE__);
			return -1;
		}

		printf("Good: didn't encrypt when:\n"
				"     1. pid is wrong\n"
				"     2. fd is right\n"
				"     3. encryption on\n\n");


		printf("Test: Same test with wrong fd.\n");

		/* Reset the file to start */
		if (-1 == lseek(fd, 0, SEEK_SET))
		{
			 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
					 "error: %s, Line:%d\n\n", strerror(errno), __LINE__);

			 return -1;
		}

		/* setting back correct pid */
		memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
		sprintf(ctrlSystemCommand, PID, ctrlPath, currentPid);

		ret = runSyscall(ctrlSystemCommand, __LINE__);
		if (0 != ret)
		{
			return ret;
		}


		/* setting wrong fd */
		memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
		sprintf(ctrlSystemCommand, FD, ctrlPath, fd+100);

		ret = runSyscall(ctrlSystemCommand, __LINE__);
		if (0 != ret)
		{
			return ret;
		}

		printf("Good: Set wrong fd icotl worked\n");

		/* check non encryption */
		memcpy(dataWrite, DATA_INPUT_2, BUFFER_SIZE);
		if (0 != writeLoop(fd, dataWrite, BUFFER_SIZE))
		{
			return -1;
		}

		/* Reset the file to start */
		if (-1 == lseek(fd, 0, SEEK_SET))
		{
			 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
					 "error: %s, Line:%d\n\n", strerror(errno), __LINE__);

			 return -1;
		}

		if (0 != readLoop(fd, dataRead, BUFFER_SIZE))
		{
			return -1;
		}

		if (0 != checkSameData(DATA_INPUT_2, dataRead, BUFFER_SIZE))
		{
			printf ("Bad: Not the same Data as expected! Line:%d\n", __LINE__);
			return -1;
		}


		printf("Good: didn't encrypt when:\n"
				"     1. pid is right\n"
				"     2. fd is wrong\n"
				"     3. encryption on\n\n");

		/***************
		 * write twice *
		 ***************/
		printf("Test: Now we will check writing twice (=2 calls to write)\n"
				"     with encryption\n");

		/* Reset the file to start */
		if (-1 == lseek(fd, 0, SEEK_SET))
		{
			 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
					 "error: %s, Line:%d\n\n", strerror(errno), __LINE__);

			 return -1;
		}


		/*** setting back the correct fd (prev test set it wrong) ***/
		memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
		sprintf(ctrlSystemCommand, FD, ctrlPath, fd);

		ret = runSyscall(ctrlSystemCommand, __LINE__);
		if (0 != ret)
		{
			return ret;
		}


		/* write 1 */
		memcpy(dataWrite, DATA_INPUT_1, BUFFER_SIZE);
		if (0 != writeLoop(fd, dataWrite, BUFFER_SIZE))
		{
			return -1;
		}

		/* write 2 */
		memcpy(dataWrite, DATA_INPUT_2, BUFFER_SIZE);
		if (0 != writeLoop(fd, dataWrite, BUFFER_SIZE))
		{
			return -1;
		}


		/***
		 * check reading with encryption (expecting same data)
		 ***/

		/* Reset the file to start */
		if (-1 == lseek(fd, 0, SEEK_SET))
		{
			 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
					 "error: %s, Line:%d\n\n", strerror(errno), __LINE__);

			 return -1;
		}

		if (0 != readLoop(fd, dataRead, BUFFER_SIZE))
		{
			return -1;
		}

		if (0 != checkSameData(DATA_INPUT_1, dataRead, BUFFER_SIZE))
		{
			printf ("Bad: Not the same Data as expected! Line:%d\n", __LINE__);
			return -1;
		}

		if (0 != readLoop(fd, dataRead, BUFFER_SIZE))
		{
			return -1;
		}

		if (0 != checkSameData(DATA_INPUT_2, dataRead, BUFFER_SIZE))
		{
			printf ("Bad: Not the same Data as expected! Line:%d\n", __LINE__);
			return -1;
		}


		/***
		 * check reading without encryption (expecting encrypted data)
		 ***/
		/* call start */
		memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
		sprintf(ctrlSystemCommand, STOP, ctrlPath);

		ret = runSyscall(ctrlSystemCommand, __LINE__);
		if (0 != ret)
		{
			return ret;
		}


		/* Reset the file to start */
		if (-1 == lseek(fd, 0, SEEK_SET))
		{
			 printf("Bad: Failed to change file pointer to the beginning of file (on lseek call), "
					 "error: %s, Line:%d\n\n", strerror(errno), __LINE__);

			 return -1;
		}

		if (0 != readLoop(fd, dataRead, BUFFER_SIZE))
		{
			return -1;
		}

		if (0 != checkSameData(DATA_INPUT_ENCRYPT_1, dataRead, BUFFER_SIZE))
		{
			printf ("Bad: Not the same Data as expected! Line:%d\n", __LINE__);
			return -1;
		}

		if (0 != readLoop(fd, dataRead, BUFFER_SIZE))
		{
			return -1;
		}

		if (0 != checkSameData(DATA_INPUT_ENCRYPT_2, dataRead, BUFFER_SIZE))
		{
			printf ("Bad: Not the same Data as expected! Line:%d\n", __LINE__);
			return -1;
		}


		printf("Good: writing twice ,with encryption, is working!!\n\n");
	}

	printf("\n!!!! Well done !!!! - the test is almost over...\n\n");


	printf("Test: Last one - removing the module\n");

	return 0;
}


/****
 * Main function.
 *
 * @notes: 1) On failure, the program do not exit cleanly (means, no memory free)
 * 		   2) The program do not deal with errors on printf calls (means, not check of return value)
 */
int main(int argc, char** argv)
{
	int success = 0;
	char resultMes[MAX_CTRL_MESSSAGE];
	int ret;

	uid_t uid, euid;

	char* ctrlPath = NULL;
	char* fileCheckPath = NULL;
	char* koPath = NULL;
	int isExtraTest = 0;

	char ctrlSystemCommand[MAX_CTRL_MESSSAGE];

	int fd = 0;

	int logFile = 0;

	/******
	 * Set command arguments
	 ******/
	if (ARGS_TOTAL_NUMBER != argc)
	{
		printf("\nUsage:\n"
				"1: [kci control execution file name (without prefix './')]\n"
				"2: [module .ko file name (with suffix of '.ko')]\n"
				"3: [file name to check on (random name)]\n"
				"4: [extra test: 0=no, 1=yes] (first run with 0, then with 1)\n\n"
				"Manual:\n"
				"  A) Common usage: 'sudo ./driver_test kci_ctrl kci_kmod.ko checkFile.txt 0'\n"
				"  B) Test info:\n"
				"      1) Before each test stage 'Test:' is printed with relevant info\n"
				"      2) After test was run: 'Good:' or 'Bad:' result is printed\n"
				"      3) After init and remove stages 'ShowFiles:' is printed with 3 params:\n"
				"        a) 'lsmod:' - looking for device name\n"
				"        b) 'device file:' - looking for device file\n"
				"        c) 'debugfs folder:' - looking for dubegfs folder\n"
				"        for all parameters (a-c): empty line == not found\n"
				"        Be aware: The tester do not check those params programlly =>\n"
				"          if the module didn't load successfully the error would be found\n"
				"          only later in the test.\n"
				"        (a,b,c are the shell commads:\n"
				"           a) lsmod | grep kci\n"
				"           b) ls /dev/ | grep kci\n"
				"           c) ls /sys/kernel/debug | grep kci)\n"
				"  C) At the end 'SUCCESS' or 'FAILURE' will be printed\n"
				"  D) Run me with sudo, Thanks.\n\n"
				"Good luck! and god bless America!\n\n");
		return -1;
	}

	/***
	 * Check if the program running in sudo
	 */
	uid = getuid();
	euid = geteuid();
	if (uid > 0 && uid == euid)
	{
		printf("Run me with sudo! please =]\n");
		return -1;
	}

	ctrlPath = argv[ARG_INDEX_OF_CTRL];
	koPath = argv[ARG_INDEX_OF_KO];
	fileCheckPath = argv[ARG_INDEX_OF_FILE_CHECK];
	isExtraTest = atoi(argv[ARG_INDEX_OF_EXTRA_TEST]);

	if (0 != isExtraTest && 1 != isExtraTest)
	{
		printf("'Extra test' argument must be 0 or 1!\n");
		return -1;
	}

	/* open in file */
	fd = open(fileCheckPath, CREATE_OVERRIDE_FILE_FOR_RW_FALGS, 0777);
	if (0 > fd)
	{
		printf("Failed to open in file for check, error %s, Line:%d\n", strerror(errno), __LINE__);
		return errno;
	}

	/*****
	 * Initialize module
	 */

	printf("Test: init module\n");

	/* call init */
	memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
	sprintf(ctrlSystemCommand, INIT, ctrlPath, koPath);

	ret = runSyscall(ctrlSystemCommand, __LINE__);
	if (0 != ret)
	{
		memset(resultMes, 0, MAX_CTRL_MESSSAGE);
		sprintf(resultMes ,"echo \"Finished: \033[31mFAILURE!!\";echo \"\e[0m\"");

		/* ingore return value */
		runSyscall(resultMes, __LINE__);

		return ret;
	}

	printf("\nGood: init module worked\nShowFiles:\n");

	/* print module data files -
	 * 		for the user to check that the module indeed was installed */
	memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
	sprintf(ctrlSystemCommand, STATUS_SHELL);

	ret = runSyscall(ctrlSystemCommand, __LINE__);
	if (0 != ret)
	{
		return ret;
	}
	/****************/

	if( 0 != mainLogic(fd, ctrlPath, isExtraTest))
	{
		printf("\n\n------- Test Failed -------\n"
				"Removing module before going down...\n\n");
	}
	else
	{
		success = 1;
	}

	/* call remove */
	memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
	sprintf(ctrlSystemCommand, REMOVE, ctrlPath);

	ret = runSyscall(ctrlSystemCommand, __LINE__);
	if (0 != ret)
	{
		return ret;
	}


	printf("Good: The module was removed successfully\nShowFiles:\n");

	/* print module data files -
	 * 		for the user to check that the module indeed was uninstalled */
	memset(ctrlSystemCommand, 0, MAX_CTRL_MESSSAGE);
	sprintf(ctrlSystemCommand, STATUS_SHELL);

	ret = runSyscall(ctrlSystemCommand, __LINE__);
	if (0 != ret)
	{
		return ret;
	}
	/****************/


	/*******
	 * Check that the log file was copied to current running dir
	 */

	printf("\nTest: Last check for a copy of the log files in current dir\n");

	logFile = open(LOG_FILE_NAME, OPEN_FILE_FOR_READING_FALGS);
	if (0 > logFile)
	{
		printf("Bad: log file failed to open from current dir, error %s\n"
								,strerror(errno));
		success = 0;
	}
	else
	{
		printf("Good: Log file is indeed in the current dir\n"
				"!!! Check manually whether the content is ok: !!!\n"
				"    there should be 3 write and 3 reads, in that order:\n"
				"	 WRITE(W) -> READ(R) -> W -> W -> R -> R\n\n");
	}


	memset(resultMes, 0, MAX_CTRL_MESSSAGE);
	sprintf(resultMes ,"echo \"Finished: %s\";echo \"\e[0m\"", (1 == success ? "\033[32mSUCCESS!!" : "\033[31mFAILURE"));

	ret = runSyscall(resultMes, __LINE__);
	if (0 != ret)
	{
		return ret;
	}

	/* free resources */
	close(fd);
	close(logFile);

	return EXIT_OS_SUCCESS;
}



int writeLoop(int fd, char* dataInputBuf, int bytesWriteCount)
{
	int bytesWritten = 0;
	int notWritten = bytesWriteCount;

	char* dataOrg = malloc(bytesWriteCount);
	memcpy(dataOrg, dataInputBuf, bytesWriteCount);

	/*****
	 * keep looping until nothing left to write to out file
	 **/
	/* at this point dataInputBuf hold the data we want to write to output */
	while (notWritten > 0)
	{
	   /* notWritten = how much we have left to write
		  bytesSent = how much we have written in current write() call */
		bytesWritten = write(fd,
						dataInputBuf + (bytesWriteCount-notWritten), /* data pointer */
						notWritten /* data size left */
							);
		if (WRITE_FAILURE == bytesWritten)
		{
			printf("Bad: Failed to write data to disc (on write), error: %s\n",
														strerror(errno));
			return errno;
		}

		notWritten -= bytesWritten;
	}

	if (-1 == checkSameData(dataOrg, dataInputBuf, bytesWriteCount))
	{
		printf("Bad: Be careful - the write command successfully finished\n"
				"But you changed the user buffer(for encrpytion)\n"
				"without returning it back original value\n");
		return -1;
	}


	free(dataOrg);
	return 0;
}

int readLoop(int fd, char* dataInputBuf, int bytesReadCount)
{
	int bytesRead = 0;
	int readTotal = 0;

	/*****
	 * keep looping until nothing left to write to server
	 * We assume (on instructions) the sever send back exactly
	 * 		the same amount of bytes which was sent to the server.
	 * 		( => no check for EOF)
	 **/
	while (readTotal < bytesReadCount)
	{
	   /* readTotal = how much we read in total
		  bytesRead = how much we have read in current read() call */
		bytesRead = read(fd,
						dataInputBuf + readTotal, /* data pointer */
						bytesReadCount - readTotal /* data size left */
							);
		if (READ_FAILURE == bytesRead)
		{
			printf("Bad: Failed to read data from server (on read), error: %s\n",
														strerror(errno));
			return errno;
		}
		else if (0 == bytesRead)
		{
			printf("Bad: Failed to get data to from file\n");
			return -1;
		}

		readTotal += bytesRead;
	}

	return 0;
}



int checkSameData(char* bufOrg, char* bufRead, int size)
{
	int i=0;

	for(;i<size;i++)
	{
		if (bufOrg[i] != bufRead[i])
		{
			return -1;
		}
	}

	return 0;
}

int runSyscall(char* ctrlSystemCommand, int lineNumber)
{
	int ret = system(ctrlSystemCommand);

	if (-1 == ret || /* system return error*/
		127 == WEXITSTATUS(ret) || /* failed to run shell command */
		2 == WEXITSTATUS(ret) /* our code return error */)
	{
		ret = WEXITSTATUS(ret);

		if (127 == ret)
		{
			printf("Bad: Bad shell run: '%s', Line: %d\n", ctrlSystemCommand,  lineNumber);
			return -1;
		}

		if (2 == ret)
		{
			printf("Info: kci_ctrl printed the error above\n\n");
			return -1;
		}

		printf("Bad: Failed to remove driver: error %s, Line:%d\n", strerror(ret), lineNumber);
		return -1;
	}

	return 0;
}
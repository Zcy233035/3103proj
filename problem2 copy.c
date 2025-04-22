#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/shm.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>

#include "helpers.h"

/**
 * @brief This function recursively traverse the source directory.
 * 
 * @param dir_name : The source directory name.
 */
void traverseDir(char *dir_name);

int main(int argc, char **argv) {
	int process_id; // Process identifier 
	
    // The source directory. 
    // It can contain the absolute path or relative path to the directory.
	char *dir_name = argv[1];

	if (argc < 2) {
		printf("Main process: Please enter a source directory name.\nUsage: ./main <dir_name>\n");
		exit(-1);
	}

	traverseDir(dir_name);

    /////////////////////////////////////////////////
    // You can add some code here to prepare before fork.
    /////////////////////////////////////////////////

	switch (process_id = fork()) {

	default:
		/*
			Parent Process
		*/
		printf("Parent process: My ID is %jd\n", (intmax_t) getpid());
        
        /////////////////////////////////////////////////
        // Implement your code for parent process here.
        /////////////////////////////////////////////////


		printf("Parent process: Finished.\n");
		break;

	case 0:
		/*
			Child Process
		*/

		printf("Child process: My ID is %jd\n", (intmax_t) getpid());

        /////////////////////////////////////////////////
        // Implement your code for child process here.
        /////////////////////////////////////////////////


		printf("Child process: Finished.\n");
		exit(0);

	case -1:
		/*
		Error occurred.
		*/
		printf("Fork failed!\n");
		exit(-1);
	}

	exit(0);
}

/**
 * @brief This function recursively traverse the source directory.
 * 
 * @param dir_name : The source directory name.
 */
void traverseDir(char *dir_name){
   
    // Implement your code here to find out
    // all textfiles in the source directory.

}
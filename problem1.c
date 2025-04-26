#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/shm.h>		// This is necessary for using shared memory constructs
#include <semaphore.h>		// This is necessary for using semaphore
#include <fcntl.h>			// This is necessary for using semaphore
#include <pthread.h>        // This is necessary for Pthread          
#include <string.h>
#include <linux/stat.h>
#include <sys/stat.h>
#include <math.h>
#include <helpers.h>
#include <time.h>
#define PARAM_ACCESS_SEMAPHORE "/param_access_semaphore"
sem_t global_sem;//the global semaphore to prevent deadlock
sem_t* dig_27_xiaotianqing[8];//the array to store all the digit_access_semaphore
const char* dig_sem[8]={"/dig0_27_xiaotianqing","/dig1_27_xiaotianqing","/dig2_27_xiaotianqing",
"/dig3_27_xiaotianqing",
"/dig4_27_xiaotianqing",
"/dig5_27_xiaotianqing",
"/dig6_27_xiaotianqing",
"/dig7_27_xiaotianqing"};//name of each digit_access_semaphore

int digits[8];//the array to reserve the digits and keep record of the modification
long int global_param = 0;
int num_of_operations;//the second parameter
/**
* This function should be implemented by yourself. It must be invoked
* in the child process after the input parameter has been obtained.
* @parms: The input parameter from the terminal.
*/
void multi_threads_run(long int input_param);

int main(int argc, char **argv)
{
	int shmid, status;
	long int local_param = 0;
	long int *shared_param_p, *shared_param_c;

	if (argc < 2) {
		printf("Please enter an eight-digit decimal number as the input parameter.\nUsage: ./main <input_param>\n");
		exit(-1);
	}
    if (argc==3){
        num_of_operations=strtol(argv[2], NULL, 10);
		printf("num_of_operations is: %d\n",num_of_operations);
    }
   	/*
		Creating semaphores. Mutex semaphore is used to acheive mutual
		exclusion while processes access (and read or modify) the global
		variable, local variable, and the shared memory.
	*/ 

	// Checks if the semaphore exists, if it exists we unlink him from the process.
	sem_unlink(PARAM_ACCESS_SEMAPHORE);
	
	// Create the semaphore. sem_init() also creates a semaphore. Learn the difference on your own.
	sem_t *param_access_semaphore = sem_open(PARAM_ACCESS_SEMAPHORE, O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 1);

	// Check for error while opening the semaphore
	if (param_access_semaphore != SEM_FAILED){
		printf("Successfully created new semaphore!\n");
	}	
	else if (errno == EEXIST) {   // Semaphore already exists
		printf("Semaphore appears to exist already!\n");
		param_access_semaphore = sem_open(PARAM_ACCESS_SEMAPHORE, 0);
	}
	else {  // An other error occured
		assert(param_access_semaphore != SEM_FAILED);
		exit(-1);
	}

	/*  
	    Creating shared memory. 
        The operating system keeps track of the set of shared memory
	    segments. In order to acquire shared memory, we must first
	    request the shared memory from the OS using the shmget()
      	system call. The second parameter specifies the number of
	    bytes of memory requested. shmget() returns a shared memory
	    identifier (SHMID) which is an integer. Refer to the online
	    man pages for details on the other two parameters of shmget()
	*/
	shmid = shmget(IPC_PRIVATE, sizeof(long int), 0666|IPC_CREAT); // We request an array of one long integer

	/* 
	    After forking, the parent and child must "attach" the shared
	    memory to its local data segment. This is done by the shmat()
	    system call. shmat() takes the SHMID of the shared memory
	    segment as input parameter and returns the address at which
	    the segment has been attached. Thus shmat() returns a char
	    pointer.
	*/

	if (fork() == 0) { // Child Process
        
		printf("Child Process: Child PID is %jd\n", (intmax_t) getpid());
		
		/*  shmat() returns a long int pointer which is typecast here
		    to long int and the address is stored in the long int pointer shared_param_c. */
        shared_param_c = (long int *) shmat(shmid, 0, 0);

		while (1) // Loop to check if the variables have been updated.
		{
			// Get the semaphore
			sem_wait(param_access_semaphore);
			printf("Child Process: Got the variable access semaphore.\n");

			if ( (global_param != 0) || (local_param != 0) || (shared_param_c[0] != 0) )
			{
				printf("Child Process: Read the global variable with value of %ld.\n", global_param);
				printf("Child Process: Read the local variable with value of %ld.\n", local_param);
				printf("Child Process: Read the shared variable with value of %ld.\n", shared_param_c[0]);

                // Release the semaphore
                sem_post(param_access_semaphore);
                printf("Child Process: Released the variable access semaphore.\n");
                
				break;
			}

			// Release the semaphore
			sem_post(param_access_semaphore);
			printf("Child Process: Released the variable access semaphore.\n");
		}

        /**
         * After you have fixed the issue in Problem 1-Q1, 
         * uncomment the following multi_threads_run function 
         * for Problem 1-Q2. Please note that you should also
         * add an input parameter for invoking this function, 
         * which can be obtained from one of the three variables,
         * i.e., global_param, local_param, shared_param_c[0].
         */
		if (sem_init(&global_sem,0,1)){
            printf("global_semaphore initialization failed.\n");
            exit(-1);
        }
		printf("global_semaphore initialized\n");
		for (int i=0;i<8;i++){
			dig_27_xiaotianqing[i] = sem_open(dig_sem[i], O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 1);
			if (dig_27_xiaotianqing[i] != SEM_FAILED){
                printf("Successfully created new digit access semaphore %d!\n",i+1);
            }	
            else if (errno == EEXIST) {   // Semaphore already exists
                printf("Semaphore appears to exist already!\n");
                dig_27_xiaotianqing[i] = sem_open(PARAM_ACCESS_SEMAPHORE, 0);
            }
            else {  // An other error occured
                assert(dig_27_xiaotianqing[i] != SEM_FAILED);
                exit(-1);
            }
        }
	
		multi_threads_run(shared_param_c[0]);

		/* each process should "detach" itself from the 
		   shared memory after it is used */

		shmdt(shared_param_c);

		exit(0);
	}
	else { // Parent Process

		printf("Parent Process: Parent PID is %jd\n", (intmax_t) getpid());
        
		/*  shmat() returns a long int pointer which is typecast here
		    to long int and the address is stored in the long int pointer shared_param_p.
		    Thus the memory location shared_param_p[0] of the parent
		    is the same as the memory locations shared_param_c[0] of
		    the child, since the memory is shared.
		*/
		shared_param_p = (long int *) shmat(shmid, 0, 0);

		// Get the semaphore first
		sem_wait(param_access_semaphore);
		printf("Parent Process: Got the variable access semaphore.\n");

		global_param = strtol(argv[1], NULL, 10);
		local_param = strtol(argv[1], NULL, 10);
		shared_param_p[0] = strtol(argv[1], NULL, 10);

		// Release the semaphore
		sem_post(param_access_semaphore);
		printf("Parent Process: Released the variable access semaphore.\n");
        
		wait(&status);

		/* each process should "detach" itself from the 
		   shared memory after it is used */

		shmdt(shared_param_p);

		/* Child has exited, so parent process should delete
		   the created shared memory. Unlike attach and detach,
		   which is to be done for each process separately,
		   deleting the shared memory has to be done by only
		   one process after making sure that noone else
		   will be using it 
		 */

		shmctl(shmid, IPC_RMID, 0);

        // Close and delete semaphore. 
        sem_close(param_access_semaphore);
        sem_unlink(PARAM_ACCESS_SEMAPHORE);
        sem_destroy(&global_sem);

        for (int i=0;i<8;i++){
            sem_close(dig_27_xiaotianqing[i]);
            sem_unlink(dig_sem[i]);
        }

		exit(0);
	}

	exit(0);
}

/**
* This function should be implemented by yourself. It must be invoked
* in the child process after the input parameter has been obtained.
* @parms: The input parameter from terminal.
*/
void* thread_job(void* arg){
    int idx=*(int*)arg;
	sem_wait(&global_sem);
    printf("Child Thread%d: Got global semaphore.\n",idx+1);
    for (int i=0;i<num_of_operations;i++){
        sem_wait(dig_27_xiaotianqing[idx]);
        sem_wait(dig_27_xiaotianqing[(idx+1)%8]);
        digits[idx]=(++digits[idx])%10;
        digits[(idx+1)%8]=(++digits[(idx+1)%8])%10;
        printf("Child Thread%d: Add digit%d to %d.\n",idx+1,idx+1,digits[idx]);
        printf("Child Thread%d: Add digit%d to %d.\n",idx+1,(idx+1)%8+1,digits[(idx+1)%8]);
        sem_post(dig_27_xiaotianqing[(idx+1)%8]);
        sem_post(dig_27_xiaotianqing[idx]);
    }
    sem_post(&global_sem);
    printf("Child Thread%d: Release global semaphore.\n",idx+1);
    usleep(rand()%150000+10000);
    return NULL;
}
void multi_threads_run(long int input_param)
{
    pthread_t threads_id[8];
	// Add your code here
    for (int i=0;i<8;i++){
        digits[i]=((int)floor((input_param/pow(10,7-i))))%10;
    }
	int thread_args[8];
	for (int i=0;i<8;i++){
		thread_args[i]=i;
		if(pthread_create(&threads_id[i],NULL,thread_job,&thread_args[i])!=0){
            printf("pthread_create fails\n");
            return;
        }
	}
    for (int i=0;i<8;i++){
        pthread_join(threads_id[i],NULL);
    }
    int sum_digits=0;
    for (int i=0;i<8;i++){
        sum_digits+=digits[i]*pow(10,7-i);
    }
    saveResult("p1_result.txt",sum_digits);
}
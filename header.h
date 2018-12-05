#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/msg.h>
#include <string.h>
#define STRUCT_ARRAY_SIZE ((sizeof(pcbArray)/sizeof(pcbArray[0])) * 18)
#define SEM_SIZE sizeof(int)
#define QUEUE_SIZE 18
//for the queue
#ifndef FALSE
#define FALSE (0)
#endif
//For true
#ifndef TRUE
#define TRUE (!FALSE)
#endif

typedef struct PCB{
	int pageTable[32];
	int isSet;
	pid_t pid;
}PCB;
struct PCB *pcbArray[18];
struct PCB *(*pcbArrPtr)[] = &pcbArray;

pid_t pid = 0;
//PCB pcbArray[18];

void createSharedMemKeys(key_t *timeKey, key_t *semKey, key_t *pcbKey){
	*timeKey = ftok(".", 2820);
	*semKey = ftok(".", 8993);
	*pcbKey = ftok(".", 4328);
};

void createSharedMemory(int *timeid, int *semid, int*pcbid, key_t timeKey, key_t semKey, key_t pcbKey){
	*timeid = shmget(timeKey, (sizeof(unsigned int) * 2), 0666 | IPC_CREAT);
	*semid = shmget(semKey, (sizeof(unsigned int) * 2), 0666 | IPC_CREAT);
	*pcbid = shmget(pcbKey, (sizeof(PCB) *2),0666|IPC_CREAT);
	if(*timeid == -1){
		printf("timeid %s\n", strerror(errno));		
	}
	if(*semid == -1){
		printf("semid %s\n", strerror(errno));		
	}
	if(*pcbid == -1){
		printf("pcbid %s\n", strerror(errno));		
	}
};

void attachToSharedMemory(unsigned int **seconds, unsigned int **nanoseconds, sem_t **semaphore, PCB **pcbPtr, int timeid, int semid, int pcbid){
	*seconds = (unsigned int*)shmat(timeid, NULL, 0);
	if(**seconds == -1){
		printf("seconds %s\n", strerror(errno));	
	}
	*nanoseconds = *seconds + 1;
	if(**nanoseconds == -1){
		printf("nanoseconds %s\n", strerror(errno));	
	}
	*semaphore = (sem_t*)shmat(semid, NULL, 0);
	if(*semaphore == (void*)-1){
		printf("semaphore %s\n", strerror(errno));	
	}
	*pcbPtr = (PCB*)shmat(pcbid, NULL, 0);
	if(*pcbPtr == (void*)-1){
		printf("pcb %s\n", strerror(errno));	
	}
};

void createArgs(char *sharedTimeMem, char *sharedSemMem, char*sharedPositionMem, char*sharedPCBMem, int timeid, int semid, int pcbid, int position){
	snprintf(sharedTimeMem, sizeof(sharedTimeMem)+25, "%d", timeid);
	snprintf(sharedSemMem, sizeof(sharedSemMem)+25, "%d", semid);
	snprintf(sharedPositionMem, sizeof(sharedPositionMem)+25, "%d", position);
	snprintf(sharedPCBMem, sizeof(sharedPCBMem)+25, "%d", pcbid);
};

pid_t forkChild(char *sharedTimeMem, char *sharedSemMem, char*sharedPositionMem, char*sharedPCBMem){
	if((pid = fork()) == 0){
		execlp("./user", "./user", sharedTimeMem, sharedSemMem, sharedPositionMem, sharedPCBMem, NULL);
	}
	if(pid < 0){
		printf("FORK Error %s\n", strerror(errno));
		//exit(0);
	}
	return pid;
	//printf("position in forkchild: %d\n", *arrayPosition);
};


#include "header.h"
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/msg.h>
#include <time.h>
#include<math.h>
#define MAXSECS 20

struct mesg_buffer { 
    long mesg_type; 
    char mesg_text[100]; 
} message; 

int alrm, processCount;
int setArr[18] = {0};

void timerKiller(int sign_no){
        alrm = 1;
}

int checkArrPosition(int *position){
        int i = 0;
	printf("Process count is %d\n", processCount);
        for(i = 0; i < processCount; i++){
		printf("setarr %d is %d\n",i, setArr[i]);
                if(setArr[i] == 0){
			setArr[i] = 1;
			printf("OSS Position %d is free\n", i);
                        *position = i;
                        return 1;
                }
        }
	
	return 0;
}

void setRandomForkTime(unsigned int *seconds, unsigned int *nanoseconds, unsigned int *forkTimeSeconds, unsigned int *forkTimeNanoseconds){
	unsigned int random = rand()%1000000000;
	*forkTimeNanoseconds = 0;
	*forkTimeSeconds = 0;	
	if((random + nanoseconds[0]) >=1000000000){
		*forkTimeSeconds += 1;
		*forkTimeNanoseconds = (random + *nanoseconds) - 1000000000;
	} else {
		*forkTimeNanoseconds = random + *nanoseconds;
	}
	*forkTimeSeconds = *seconds + rand()%2;
}

int main(int argc, char *argv[]){
	srand(time(NULL));
	char* filename = malloc(sizeof(char));
	filename = "log.txt";
	FILE *logFile = fopen(filename, "a");
	char g;
	processCount = 18;
	int frameTable[256][3];
	while((g = getopt(argc, argv, "hp:")) != -1){
		switch(g){
			case'h': 
				printf("Arguments:\n-p: Number of user processes spawned (default 18)\n");
                                exit(0);
                        case'p': 
				processCount = atoi(optarg);
				if (processCount > 18){
				 	processCount = 18;
				}                                
				break;
                }
	}

	key_t msgKey = ftok(".", 432820), timeKey = 0, pcbKey = 0, semKey = 0;
	int msgid = msgget(msgKey, 0666 | IPC_CREAT), timeid = 0, pcbid = 0, semid = 0, position = 0;
	printf("OSS msgid %d\n", msgid);
	unsigned int *seconds = 0, *nanoseconds = 0, forkTimeSeconds = 0, forkTimeNanoseconds = 0;
	PCB *pcbPtr = NULL;
	sem_t *semPtr = NULL;
	
	char sharedTimeMem[10], sharedPCBMem[10], sharedSemMem[10], sharedPositionMem[10];
	
	createSharedMemKeys(&timeKey, &semKey, &pcbKey);
 	createSharedMemory(&timeid, &semid, &pcbid, timeKey, semKey, pcbKey);
	attachToSharedMemory(&seconds, &nanoseconds, &semPtr, &pcbPtr, timeid, semid, pcbid);
	//create memkeys, memory, attach
	//msgrcv(msgid, &message, sizeof(message), pcbPtr[i].pid, 0);
	int forked = 0, forkTimeSet = 0, i = 0, tempPid = 0;
	float childRequestAddress = 0;
	char childMsg[20];
	//sem_wait(semPtr);
	//signal(SIGALRM, timerKiller);
	//alarm(2);
	do{

		if(forkTimeSet == 0){
			setRandomForkTime(seconds, nanoseconds, &forkTimeSeconds, &forkTimeNanoseconds);
			forkTimeSet = 1;
			printf("OSS: Fork time set for %d.%d\n", forkTimeSeconds, forkTimeNanoseconds);
		}
		*nanoseconds += 5000;
		if(*nanoseconds >= 1000000000){
			*seconds += 1;
			*nanoseconds = 0;
		}
		if(((*seconds == forkTimeSeconds) && (*nanoseconds >= forkTimeNanoseconds)) || (*seconds > forkTimeSeconds)){
			//sleep(1);
			if(checkArrPosition(&position) == 1){			
				printf("BEGIN==================\n");
				forked++;
				forkTimeSet = 0;
				printf("forking at %d : %d \n", *seconds, *nanoseconds);
				createArgs(sharedTimeMem, sharedSemMem, sharedPositionMem, sharedPCBMem, timeid, semid, pcbid, position);
				pid_t childPid = forkChild(sharedTimeMem, sharedSemMem, sharedPositionMem, sharedPCBMem);
				pcbArray[position] = malloc(sizeof(struct PCB));
				(*pcbArrPtr)[position]->pid = childPid;
				printf("Child pid is %d\n", childPid);
				for(i = 0 ; i < 32; i++){
					(*pcbArrPtr)[position]->pageTable[i] = -1;
				}
				(*pcbArrPtr)[position]->isSet = 1; // pointer to an array os pointers to structsd
				
				//printf("atoi cpd %d\n", atoi(childPID));
				printf("FORKED %d\n", forked);
				printf("END==================\n");
				//sleep(1);
			}
		}
		if(processCount < 18){
			printf("PROCESS COUNT IS LESS THAN 18 AT %d\n", processCount);
		}
		for(i = 0; i < processCount; i++){
			
			if(setArr[i] == 1){
				//printf("position %d pid %d ", i, (*pcbArrPtr)[i]->pid);
				tempPid =  (*pcbArrPtr)[i]->pid;
				//printf("TEST 1 process count %d\n", processCount);
				//printf("OSS checking for messages of type %ld\n", (*pcbArrPtr)[i]->pid);
				//sleep(1);
				if(msgrcv(msgid, &message, sizeof(message)-sizeof(long), tempPid, IPC_NOWAIT) > 0){
					printf("OSS received message %d from position %d\n", atoi(message.mesg_text), position);
					if(atoi(message.mesg_text) != 99999){ //it received  aread or write
						printf("TEST 2 process count %d\n", processCount);
						printf("OSS 2 message text %s child pid %d\n", message.mesg_text, (*pcbArrPtr)[i]->pid);
						printf("TEST 3 process count %d\n", processCount);
						strcpy(childMsg, message.mesg_text);
						printf("OSS child message %d\n", atoi(childMsg));
						childRequestAddress = (atoi(childMsg))/1000;
						childRequestAddress = (int)(floor(childRequestAddress));
						printf("child request address is %d\n", (int)childRequestAddress);
						if((*pcbArrPtr)[i]->pageTable[(int)childRequestAddress] == -1){
							//assign to Frame Table
							//if frame table at frameTable[childRequestAddress][0]
							frameTable[0][0] = childRequestAddress; 
							printf("Frame table [0][0] is %d\n", (int)childRequestAddress);
						}
					} else if(atoi(message.mesg_text) == 99999){ //it received a death signal
						setArr[i] = 0; //basically if it received a message then it wants to die
						message.mesg_type = ((*pcbArrPtr)[i]->pid+118);
						//sleep(5);
						printf("OSS sent death message of type %d\n", (*pcbArrPtr)[i]->pid+118);
						sprintf(message.mesg_text,"1");
						msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
						(*pcbArrPtr)[i]->isSet = 0;
						(*pcbArrPtr)[i]->pid = 0;
					}
				}
			}
		}
		/*for( i = 0; i < processCount; i++){
			if(setArr[i] == 1){
				//printf("EWFASDFASGFEWASGFSDFASRGASD\n");
				//printf("OSS checking for messages from %d on %d\n", i, (*pcbArrPtr)[i]->pid + 18);
				if(msgrcv(msgid, &message, sizeof(message)-sizeof(long), ((*pcbArrPtr)[i]->pid + 18), IPC_NOWAIT) > 0){
					setArr[i] = 0; //basically if it received a message then it wants to die
					message.mesg_type = ((*pcbArrPtr)[i]->pid+118);
					sleep(5);
					printf("OSS sent death message of type %d\n", (*pcbArrPtr)[i]->pid+118);
					sprintf(message.mesg_text,"1");
					msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
					(*pcbArrPtr)[i]->isSet = 0;
					(*pcbArrPtr)[i]->pid = 0;
					
				}
			}
		}*/
		
	}while((*seconds < MAXSECS+10000) && alrm == 0 && forked < 100);
	printf("Main has left the building it forked %d process count was %d\n ", forked, processCount);
	for(i = 0; i < processCount; i++){
		if(setArr[i] == 1){
			printf("OSS user pid is %d\n", (*pcbArrPtr)[i]->pid);
		}	
	}
	printf("fram table 0 = %d\n", frameTable[0][0]); 
	
	fclose(logFile);
	shmdt(seconds);
	shmdt(semPtr);
	shmdt(pcbPtr);
	msgctl(msgid, IPC_RMID, NULL);
	shmctl(msgid, IPC_RMID, NULL);
	shmctl(pcbid, IPC_RMID, NULL);
	shmctl(timeid, IPC_RMID, NULL);
	shmctl(semid, IPC_RMID, NULL);
	return 0;
}



		




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

int alrm, processCount, frameTablePos = 0;
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
	
	int forked = 0, forkTimeSet = 0, i = 0, tempPid = 0, status, frameLoop = 0, pagefault = 0;
	float childRequestAddress = 0;
	char childMsg[20];
	char requestType[20];
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
				
				printf("FORKED %d\n", forked);
				printf("END==================\n");
			}
		}
		for(i = 0; i < processCount; i++){
			
			if(setArr[i] == 1){
				
				tempPid =  (*pcbArrPtr)[i]->pid;
				
				if((msgrcv(msgid, &message, sizeof(message)-sizeof(long), tempPid, IPC_NOWAIT|MSG_NOERROR)) > 0){
					printf("OSS received message %d from position %d\n", atoi(message.mesg_text), position);
					if(atoi(message.mesg_text) != 99999){ //it received  aread or write	
						strcpy(childMsg, strtok(message.mesg_text, " "));
						strcpy(requestType, strtok(NULL, " "));
						printf("OSS child message %d\n", atoi(childMsg));
						childRequestAddress = (atoi(childMsg))/1000;
						childRequestAddress = (int)(floor(childRequestAddress));
						printf("child request address is %d\n", (int)childRequestAddress);
						if((*pcbArrPtr)[i]->pageTable[(int)childRequestAddress] == -1){
							//assign to Frame Table
							//need to check if pagetable[childrequestaddress] isnt -1;
							//if frame table at frameTable[childRequestAddress][0]
							frameLoop = 0;
							while(frameTable[frameTablePos][0] != 0 && frameLoop < 255){
								frameTablePos++;
								frameLoop++;
								if(frameTablePos == 256){
									frameTablePos = 0;
								} 
								if(frameLoop == 255){
									pagefault = 1;
								}
							}
							if(pagefault == 1){
								while(frameTable[frameTablePos][1] != 1){ //checks for frame where second chance bit isnt set
									frameTablePos++;
									if(frameTablePos == 256){
										frameTablePos = 0;
									}
								} //assuming it finds a place where R is 0
								if(frameTable[frameTablePos][1] == 0){
									//new page goes here
									(*pcbArrPtr)[i]->pageTable[(int)childRequestAddress] = frameTablePos;
									frameTable[frameTablePos][0] = 1;
									frameTable[frameTablePos][1] = 1;//R is set
									frameTable[frameTablePos][2] = 1;//D is set
								}
							} else {
								(*pcbArrPtr)[i]->pageTable[(int)childRequestAddress] = frameTablePos; //otherwise, 
								frameTable[frameTablePos][0] = 1;
								frameTable[frameTablePos][1] = 0;//R is cleared
								frameTable[frameTablePos][2] = 1;//D is set
								frameTablePos++; //clock advances.
							}
							if(frameTable[frameTablePos][0] == 0){
								if(frameTable[frameTablePos][0] == 0){
									frameTable[frameTablePos][0] = (int)childRequestAddress;
										
							//(*pcbArrPtr)[i]->pageTable[(int)childRequestAddress)] is equal to the first open spot
							
							frameTable[0][0] = childRequestAddress; 
							printf("Frame table [0][0] is %d\n", (int)childRequestAddress);
						}
						message.mesg_type = ((*pcbArrPtr)[i]->pid+118);
						sprintf(message.mesg_text,"wakey");
						msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
	
					} else if(atoi(message.mesg_text) == 99999){ //it received a death signal
						setArr[i] = 0; //basically if it received a message then it wants to die						
						message.mesg_type = ((*pcbArrPtr)[i]->pid+118);
						printf("OSS sent death message of type %d\n", (*pcbArrPtr)[i]->pid+118);
						sprintf(message.mesg_text,"wakey");
						msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
						waitpid(((*pcbArrPtr)[i]->pid), &status, 0);
						free(pcbArray[i]);
					}
				} else {
					//printf("Message size %d\n",( sizeof(message) - sizeof(long)));
					//printf("ERROR %d %s\n",errno,  strerror(errno));
					//exit(1);
				}
			}
		}
		
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



		




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
#include <signal.h>
#define MAXSECS 100

void segfault_sigaction(int signal, siginfo_t *si, void *arg){
        fprintf(stderr, "Caught segfault at address %p\n", si->si_addr);
        kill(0, SIGTERM);
}

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
	//printf("Process count is %d\n", processCount);
        for(i = 0; i < processCount; i++){
		//printf("setarr %d is %d\n",i, setArr[i]);
                if(setArr[i] == 0){
			setArr[i] = 1;
			//printf("OSS Position %d is free\n", i);
                        *position = i;
                        return 1;
                }
        }
	
	return 0;
}

void setRandomForkTime(unsigned int *seconds, unsigned int *nanoseconds, unsigned int *forkTimeSeconds, unsigned int *forkTimeNanoseconds){
	unsigned int random = rand()%500000000;//1000000000;
	*forkTimeNanoseconds = 0;
	*forkTimeSeconds = 0;	
	if((random + nanoseconds[0]) >=1000000000){
		*forkTimeSeconds += 1;
		*forkTimeNanoseconds = (random + *nanoseconds) - 1000000000;
	} else {
		*forkTimeNanoseconds = random + *nanoseconds;
	}
	*forkTimeSeconds = *seconds;// + rand()%2;
}

void printFrameTable(int frameTable[][3], FILE*file){
	int i;
	for(i = 0; i < 256; i++){
		if(frameTable[i][0] == 0){
			fprintf(file,".");
		}else if(frameTable[i][0] != 0 && frameTable[i][2] == 0){
			fprintf(file,"U");
		}else {
			fprintf(file,"D");
		}
	}
	fprintf(file,"\n");
	for(i = 0; i < 256; i++){
                if(frameTable[i][0] == 0){
                       fprintf(file,".");
                }else{
			fprintf(file,"%d", frameTable[i][1]);
		}
        }
	fprintf(file,"\n");
}

int main(int argc, char *argv[]){
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = segfault_sigaction;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV,&sa,NULL);

	srand(time(NULL));
	char* filename = malloc(sizeof(char));
	filename = "log.txt";
	FILE *logFile = fopen(filename, "w");
	freopen("log.txt","a",logFile);
	char g;
	processCount = 18;
	int percentage = 50, limit = 900;
	int frameTable[256][3] = {{0}};
	while((g = getopt(argc, argv, "hp:x:l:")) != -1){
		switch(g){
			case'h': 
				printf("Arguments:\n-p: Number of user processes spawned (default 18)\n-x: Percentage of read requests (default 50). Write requests take up the remaining percentage. Accepts 0-99.\n-l: Changes the limit of requests made before a process checks for a 75 percent chance of termination (default 900). Accepts 1-2000.\n");
                                exit(0);
                        case'p': 
				processCount = atoi(optarg);
				if (processCount > 18){
				 	processCount = 18;
				}                                
				//break;
			case'x': 
				percentage = atoi(optarg);
				if(percentage > 99 || percentage < 0){
					percentage = 50;
				}
				//break;
			case'l':
				limit = atoi(optarg);
				if(limit < 0 || limit > 2000){
					limit = 900;
				}	
                }
	}

	key_t msgKey = ftok(".", 432820), timeKey = 0, pcbKey = 0, semKey = 0;
	int msgid = msgget(msgKey, 0666 | IPC_CREAT), timeid = 0, pcbid = 0, semid = 0, position = 0;
	//printf("OSS msgid %d\n", msgid);
	unsigned int *seconds = 0, *nanoseconds = 0, forkTimeSeconds = 0, forkTimeNanoseconds = 0, accessSpeed = 0;
	PCB *pcbPtr = NULL;
	sem_t *semPtr = NULL;
	
	char sharedTimeMem[10], sharedPCBMem[10], sharedSemMem[10], sharedPositionMem[10], sharedPercentageMem[10], sharedLimitMem[10];
	
	createSharedMemKeys(&timeKey, &semKey, &pcbKey);
 	createSharedMemory(&timeid, &semid, &pcbid, timeKey, semKey, pcbKey);
	attachToSharedMemory(&seconds, &nanoseconds, &semPtr, &pcbPtr, timeid, semid, pcbid);
	
	int forked = 0,lines = 0, forkTimeSet = 0,j = 0, i = 0,tempPid = 0, status, frameLoop = 0, pagefault = 0, requests = 0;
	double pageFaults = 0, memoryAccesses = 0, memoryAccessesPerSecond = 0;
	float childRequestAddress = 0;
	char childMsg[20], ch;
	char requestType[20];
	int address;
	
	signal(SIGALRM, timerKiller);
	alarm(2);
	do{

		if(forkTimeSet == 0){
			setRandomForkTime(seconds, nanoseconds, &forkTimeSeconds, &forkTimeNanoseconds);
			forkTimeSet = 1;
			fprintf(logFile, "Master: Fork time set for %d : %d\n", forkTimeSeconds, forkTimeNanoseconds);
		}
		*nanoseconds += 50000;
		if(*nanoseconds >= 1000000000){
			*seconds += 1;
			*nanoseconds = 0;
			memoryAccessesPerSecond = (memoryAccesses/ *seconds);
		}
		if(((*seconds == forkTimeSeconds) && (*nanoseconds >= forkTimeNanoseconds)) || (*seconds > forkTimeSeconds)){
			if(checkArrPosition(&position) == 1){			
				//printf("BEGIN==================\n");
				forked++;
				forkTimeSet = 0;
				fprintf(logFile,"Master: forking at %d : %d \n", *seconds, *nanoseconds);
				createArgs(sharedTimeMem, sharedSemMem, sharedPositionMem, sharedPCBMem, sharedLimitMem, sharedPercentageMem, timeid, semid, pcbid, position, limit, percentage);
				pid_t childPid = forkChild(sharedTimeMem, sharedSemMem, sharedPositionMem, sharedPCBMem, sharedLimitMem, sharedPercentageMem);
				pcbArray[position] = malloc(sizeof(struct PCB));
				(*pcbArrPtr)[position]->pid = childPid;
				
				fprintf(logFile,"Master: Child %d was made with pid %d\n", position, childPid);
				for(i = 0 ; i < 32; i++){
					(*pcbArrPtr)[position]->pageTable[i] = -1;
				}
				(*pcbArrPtr)[position]->isSet = 1; // pointer to an array os pointers to structsd
				
				//printf("FORKED %d\n", forked);
				//printf("END==================\n");
			}
		}
		for(i = 0; i < processCount; i++){
			
			if(setArr[i] == 1){
				
				tempPid =  (*pcbArrPtr)[i]->pid;
				
				if((msgrcv(msgid, &message, sizeof(message)-sizeof(long), tempPid, IPC_NOWAIT|MSG_NOERROR)) > 0){
					//printf("OSS received message %d from position %d\n", atoi(message.mesg_text), position);
					if(atoi(message.mesg_text) != 99999){ //it received  aread or write
						fprintf(logFile, "Master: process P%d requesting address %d to ",i ,atoi(message.mesg_text));	
						strcpy(childMsg, strtok(message.mesg_text, " "));
						address = atoi(childMsg);
						strcpy(requestType, strtok(NULL, " "));
						if(atoi(requestType) == 0){
							fprintf(logFile, "read at time %d : %d\n", *seconds, *nanoseconds);
						}else{
							fprintf(logFile, "write at time %d : %d\n", *seconds, *nanoseconds);
						} 
						//printf("OSS child message %d\n", atoi(childMsg));
						childRequestAddress = (atoi(childMsg))/1000;
						childRequestAddress = (int)(floor(childRequestAddress));
						//printf("child request address is %d\n", (int)childRequestAddress);
						if((*pcbArrPtr)[i]->pageTable[(int)childRequestAddress] == -1 || frameTable[(*pcbArrPtr)[i]->pageTable[(int)childRequestAddress]][0] != (*pcbArrPtr)[i]->pid){//if the page table position is empty or the pagetable frame position no longer is associated with the child request address
							//assign to Frame Table
							//need to check if pagetable[childrequestaddress] isnt -1;
							//if frame table at frameTable[childRequestAddress][0]
							frameLoop = 0;
							while(frameTable[frameTablePos][0] != 0 && frameLoop < 255){ //check for first empty frame
								frameTablePos++;
								frameLoop++;
								if(frameTablePos == 256){
									frameTablePos = 0;
								} 
								if(frameLoop == 255){
									pagefault = 1;
								}
							}
							if(pagefault == 1){ //if no frames were empty
								pageFaults++;
								fprintf(logFile, "Master: Address %d is not in a frame, pagefault\n", address);
								while(frameTable[frameTablePos][1] != 0){ //checks for frame where second chance bit isnt set
									frameTable[frameTablePos][1] = 0; //set to 0 if it was 1 
									frameTablePos++; //then move position
									if(frameTablePos == 256){
										frameTablePos = 0;
									}
								} //assuming it finds a place where R is 0
								if(frameTable[frameTablePos][1] == 0){
									memoryAccesses++;
									fprintf(logFile, "Master: clearing frame %d and swapping in P%d page %d\n", frameTablePos, i, (int)childRequestAddress);
									//new page goes here
									(*pcbArrPtr)[i]->pageTable[(int)childRequestAddress] = frameTablePos;
									frameTable[frameTablePos][0] = (*pcbArrPtr)[i]->pid;//(int)childRequestAddress;
									frameTable[frameTablePos][2] = atoi(requestType); 
									fprintf(logFile, "Master: Address %d in frame %d giving data to P%d at time %d : %d\n", address, frameTablePos, i, *seconds, *nanoseconds);
									frameTablePos++; //clock advances
									if(frameTablePos == 256){
                                                                                frameTablePos = 0;
                                                                        }
									requests++;
								}
								accessSpeed +=  15000000;
								*nanoseconds += 15000000;	
								fprintf(logFile, "Master: Dirty bit is set to %d and clock is incremented 15ms\n", atoi(requestType)); 
							} else { //if it finds a place with an empty frame
								memoryAccesses++;
								(*pcbArrPtr)[i]->pageTable[(int)childRequestAddress] = frameTablePos; //otherwise, 
								frameTable[frameTablePos][0] = (*pcbArrPtr)[i]->pid;//(int)childRequestAddress;
								frameTable[frameTablePos][1] = 0;//R is cleared
								frameTable[frameTablePos][2] = atoi(requestType);
								fprintf(logFile, "Master: Address %d in frame %d giving data to P%d at time %d : %d\n", address, frameTablePos, i, *seconds, *nanoseconds);
								frameTablePos++; //clock advances.
								if(frameTablePos == 256){
                                                                	frameTablePos = 0;
                                                                }
								accessSpeed  += 10000000;
								*nanoseconds += 10000000;
								requests++;
								fprintf(logFile, "Master: Dirty bit is set to %d and clock is incremented 10ms\n", atoi(requestType));
							}
										
							//(*pcbArrPtr)[i]->pageTable[(int)childRequestAddress)] is equal to the first open spo
						} else {
							memoryAccesses++;
							frameTable[(*pcbArrPtr)[i]->pageTable[(int)childRequestAddress]][1] = 1; //reference bit set
							frameTable[(*pcbArrPtr)[i]->pageTable[(int)childRequestAddress]][2] = atoi(requestType); //Dirty Bit is set MAYBE CHANGE THIS TO PROCESS NUMBER
							*nanoseconds += 10000000;
							accessSpeed +=  10000000;
							requests++; ///////////////////////////////////////////////////////////////////////////////
							fprintf(logFile, "Master: Dirty bit is set to %d and clock is incremented 10ms\n", atoi(requestType));	
						}

						message.mesg_type = ((*pcbArrPtr)[i]->pid+118);
						sprintf(message.mesg_text,"wakey"); 
						msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
	
					} else if(atoi(message.mesg_text) == 99999){ //it received a death signal
						setArr[i] = 0; //basically if it received a message then it wants to die						
						message.mesg_type = ((*pcbArrPtr)[i]->pid+118);
						//printf("OSS sent death message of type %d\n", (*pcbArrPtr)[i]->pid+118);
						fprintf(logFile, "Master: P%d is finishing, clearing frames: ", i);
						for(j = 0; j < 32; j++){
							
							if((*pcbArrPtr)[i]->pageTable[j] != -1 && frameTable[(*pcbArrPtr)[i]->pageTable[j]] == (*pcbArrPtr)[i]->pageTable[j]){
								fprintf(logFile, "%d, ", j);
								frameTable[(*pcbArrPtr)[i]->pageTable[j]][0] = 0;
								frameTable[(*pcbArrPtr)[i]->pageTable[j]][1] = 0;
								frameTable[(*pcbArrPtr)[i]->pageTable[j]][2] = 0;
								(*pcbArrPtr)[i]->pageTable[j] = -1;
							}
						} 
						fprintf(logFile,"\n");
						sprintf(message.mesg_text,"wakey");
						msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
						waitpid(((*pcbArrPtr)[i]->pid), &status, 0);
						free(pcbArray[i]);
					}
					if(requests >= 100){
						printFrameTable(frameTable, logFile);
						requests = 0;
					}
				} else {
					//fprintf(stderr,"ERROR %d %s\n",errno, strerror(errno));
					//fclose(logFile);
        				//shmdt(seconds);
        				//shmdt(semPtr);
        				//shmdt(pcbPtr);
        				//msgctl(msgid, IPC_RMID, NULL);
        				//shmctl(msgid, IPC_RMID, NULL);
        				//shmctl(pcbid, IPC_RMID, NULL);
        				//shmctl(timeid, IPC_RMID, NULL);
       	 				//shmctl(semid, IPC_RMID, NULL);
	        			//kill(0, SIGTERM);
					//exit(0);
				}
			}
		}
		while((ch = fgetc(logFile)) != EOF){
			if(ch == '\n'){
				lines++;
			}
		}
		if(lines >= 100000){
			fclose(logFile);
		}
				
	}while((*seconds < MAXSECS+10000) && alrm == 0 && forked < 100);
	fprintf(logFile, "Master: \n\tFINISHED With %f memory accesses per second.\n\t%f pagefaults per memory access.\n\t%f average access speed in nanoseconds.\n\t%d forks.\n", memoryAccessesPerSecond, pageFaults/memoryAccesses, accessSpeed/memoryAccesses, forked);
	//printf("Main has left the building it forked %d process count was %d\n ", forked, processCount);

	//printf("fram table 0 = %d\n", frameTable[0][0]); 
	
	fclose(logFile);
	shmdt(seconds);
	shmdt(semPtr);
	shmdt(pcbPtr);
	msgctl(msgid, IPC_RMID, NULL);
	shmctl(msgid, IPC_RMID, NULL);
	shmctl(pcbid, IPC_RMID, NULL);
	shmctl(timeid, IPC_RMID, NULL);
	shmctl(semid, IPC_RMID, NULL);
	kill(0, SIGTERM);
	return 0;
}

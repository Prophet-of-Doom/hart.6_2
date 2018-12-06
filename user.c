#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <sys/msg.h> 
 
#include "header.h"
struct mesg_buffer { 
    long mesg_type; 
    char mesg_text[100]; 
} message; 
void setRandomEventTime(unsigned int *seconds, unsigned int *nanoseconds, unsigned int *eventTimeSeconds, unsigned int *eventTimeNanoseconds){
	unsigned int random = rand()%500000000;//1000000000;
	*eventTimeNanoseconds = 0;
	*eventTimeSeconds = 0;	
	if((random + *nanoseconds) >=1000000000){
		*eventTimeSeconds += 1;
		*eventTimeNanoseconds = (random + *nanoseconds) - 1000000000;
	} else {
		*eventTimeNanoseconds = random + *nanoseconds;
	}
	*eventTimeSeconds = *seconds;// + rand()%2;
}
int main(int argc, char *argv[]) {
	srand(getpid()); 	
        key_t msgKey = ftok(".",432820);
	int msgid = msgget(msgKey, 0666 | IPC_CREAT);
	//printf("Message type : %d\n", msgid);
	int timeid = atoi(argv[1]);
	int semid = atoi(argv[2]);
	int position = atoi(argv[3]);
	int pcbid = atoi(argv[4]);
	int limit = atoi(argv[5]);
	int percentage = atoi(argv[6]);
	int event = 0;	
	pid_t pid = getpid();
	sem_t *semPtr;
	PCB *pcbPtr;
	unsigned int *seconds = 0, *nanoseconds = 0, eventTimeSeconds = 0, eventTimeNanoseconds = 0, requests = 0;
	attachToSharedMemory(&seconds, &nanoseconds, &semPtr, &pcbPtr, timeid, semid, pcbid);
	//printf("\nUSER position: %d PID: %d\n", position, pid);
	//printf("USER message id %d\n", msgid);
	message.mesg_type = pid;
	int complete = 0, request = 0;
	
	message.mesg_type = 12345;//(int)pid;
	setRandomEventTime(seconds, nanoseconds, &eventTimeSeconds, &eventTimeNanoseconds);
	//printf("Limit is %d\n", limit);
	while(complete == 0){
		if((*seconds == eventTimeSeconds && *nanoseconds >= eventTimeNanoseconds) || *seconds > eventTimeSeconds){
			event = rand()%99;//99;
			request = rand()%32001;
			requests++;
			setRandomEventTime(seconds, nanoseconds, &eventTimeSeconds, &eventTimeNanoseconds);
			if(requests == limit/*900*/ && event < 75){
				//death
				message.mesg_type = (int)pid;
				sprintf(message.mesg_text,"%d", 99999);
				msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
				//printf("USER %d SENT DEATH MESSAGE and is waiting for message at %d\n",position, pid + 118);
				msgrcv(msgid, &message, sizeof(message)-sizeof(long), (pid+118), 0);
				//printf("USER CAN DIE\n");
				complete = 1;
			} else if(event < percentage/*50*/){
				//read
				message.mesg_type = (int)pid;
				sprintf(message.mesg_text,"%d %d", request, 0);
				msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
				//printf("USER %d SENT REQUEST READ MESSAGE and is waiting for message at %d\n",position, pid + 118);
				msgrcv(msgid, &message, sizeof(message)-sizeof(long), (pid+118), 0);
				//printf("USER CAN CONTINUE\n");
			} else if(event >= (99-percentage)/*50*/){
				//write
				message.mesg_type = (int)pid;
                                sprintf(message.mesg_text,"%d %d", request, 1);
                                msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
                               // printf("USER %d SENT WRITE MESSAGE and is waiting for message at %d\n",position, pid + 118);
                                msgrcv(msgid, &message, sizeof(message)-sizeof(long), (pid+118), 0);
                                //printf("USER CAN CONTINUE\n");
			}
		}
	}
	
	shmdt(seconds);     	
	shmdt(semPtr);
	shmdt(pcbPtr);
	
	shmctl(msgid, IPC_RMID, NULL);
	
	exit(0);
}


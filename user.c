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
	unsigned int random = rand()%1000000000;
	*eventTimeNanoseconds = 0;
	*eventTimeSeconds = 0;	
	if((random + *nanoseconds) >=1000000000){
		*eventTimeSeconds += 1;
		*eventTimeNanoseconds = (random + *nanoseconds) - 1000000000;
	} else {
		*eventTimeNanoseconds = random + *nanoseconds;
	}
	*eventTimeSeconds = *seconds + rand()%2;
}
int main(int argc, char *argv[]) {
	srand(time(NULL)); 	
        key_t msgKey = ftok(".",432820);
	int msgid = msgget(msgKey, 0666 | IPC_CREAT);
	//printf("Message type : %d\n", msgid);
	int timeid = atoi(argv[1]);
	int semid = atoi(argv[2]);
	int position = atoi(argv[3]);
	int pcbid = atoi(argv[4]);
	int event = 0, eventResource = 0;	
	pid_t pid = getpid();
	sem_t *semPtr;
	PCB *pcbPtr;
	char received[20];	
	unsigned int *seconds = 0, *nanoseconds = 0, eventTimeSeconds = 0, eventTimeNanoseconds = 0;
	attachToSharedMemory(&seconds, &nanoseconds, &semPtr, &pcbPtr, timeid, semid, pcbid);
	printf("\nUSER position: %d PID: %d\n", position, pid);
	printf("USER message id %d\n", msgid);
	message.mesg_type = pid;
	int complete = 0, request = 0;
	//printf("USER request is %d\n", request);
	//sprintf(message.mesg_text, "%d", request);
	//printf("USER message is %d\n", atoi(message.mesg_text));
	//msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);

	//sem_wait(semPtr);
	//message.mesg_type = (position + 8180);
	//sprintf(message.mesg_text,"%d", pid);
	//msgsnd(msgid, &message, sizeof(message), 0);
	
	//if((msgrcv(msgid, &message, sizeof(message), (position + 4328), 0)) < 0){
	//	printf("%s\n", strerror(errno));
	//}
	//printf("\nUSER: AFTER MSG SEND\n");
	//(*pcbArrPtr)[position]->pid = pid;
	//printf("USER: position %d pid %d\n", position, (*pcbArrPtr)[position]->pid);
	//printf("USER: AFTER msgsnd %d\n", position);
	//printf("USER: position %d pid %d\n", position, pid);
	//printf("\nUSER: seconds are: %u nano are: %u position %d\n", *seconds, *nanoseconds, position);
	message.mesg_type = (int)pid;
	setRandomEventTime(seconds, nanoseconds, &eventTimeSeconds, &eventTimeNanoseconds);
	while(complete == 0){
		//if((*seconds == eventTimeSeconds && *nanoseconds >= eventTimeNanoseconds) || *seconds > eventTimeSeconds){
			event = rand()%9;//99;
			request = rand()%32001;
			if(event >= 0 && event < 10){
				//death
				message.mesg_type = (pid + 18);
				sprintf(message.mesg_text,"%d", 99999);
				msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
				printf("USER SENT MESSAGE and is waiting for message at %d\n", pid + 118);
				msgrcv(msgid, &message, sizeof(message)-sizeof(long), (pid+118), 0);
				printf("USER CAN DIE\n");
				complete = 1;
			} else if(event >= 10 && event < 65){
				//request
			} else if(event >= 65){
				//read
			}
		//}
	}
	
	//message.mesg_type = (pid + 18);
	//sprintf(message.mesg_text,"1");
	//msgsnd(msgid, &message, sizeof(message)-sizeof(long), 0);
	//printf("USER SENT MESSAGE\n");
	
	//printf("USER going to receive message of type %d\n", (pid+118));
	
	//msgrcv(msgid, &message, sizeof(message)-sizeof(long), (pid+118), 0);
		
	
	shmdt(seconds);     	
	shmdt(semPtr);
	shmdt(pcbPtr);
	
	msgctl(msgid, IPC_RMID, NULL);
	shmctl(msgid, IPC_RMID, NULL);
	shmctl(timeid, IPC_RMID, NULL);
	shmctl(semid, IPC_RMID, NULL);
	shmctl(pcbid, IPC_RMID, NULL);
	return 0;
}

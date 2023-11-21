#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define buf 1000*sizeof(int)

key_t key;
char seqNum[4];
sem_t mutex[20];
int operation;
int msqid;

struct message {
    long mtype;
    char mtext[100];
};
typedef struct message message;

void *makeModify(void *fileName)
{
    key_t newkey=ftok("client.c", 'B');
    int shmid = shmget(newkey, buf, 0666);
    if (shmid == -1) 
    {
        perror("Error getting shared memory key for numNodess");
        pthread_exit(0);
    }
    int *shmptr = (int *)shmat(shmid,NULL,0);
    int numNode = *shmptr;
    int num_sem = atoi(fileName + 1)-1;
    sem_wait(&mutex[num_sem]);
    FILE * file = fopen(fileName,"w");
    if (file == NULL) {
        perror("Error opening file");
        exit(0);
    }
    shmptr++;
    int adjMatrix[numNode][numNode];
    fprintf(file,"%d\n",numNode);
    for (int i = 0; i < numNode; i++) {
        for (int j = 0; j < numNode; j++) {
            adjMatrix[i][j]= *shmptr;
            fprintf(file,"%d ",adjMatrix[i][j]);
            shmptr++;
        }
        fprintf(file,"\n");
    }
    if(operation==1) printf("File added successfully\n");
    else printf("File modified successfully\n");
    fclose(file);
    sem_post(&mutex[num_sem]);
    shmdt(shmptr);
    message successMessage;
    successMessage.mtype = 1000 + atoi(seqNum);
    if(operation==1) strcpy(successMessage.mtext,"Succesfully Added file\n");
    else strcpy(successMessage.mtext,"Succesfully Modified file\n");
    if (msgsnd(msqid, &successMessage, sizeof(successMessage.mtext), 0) == -1) 
    {
        perror("Error sending message to the client");
        exit(EXIT_FAILURE);
    }
    pthread_exit(0);
}

int main() {
    int shmid;

    key = ftok("load_balancer.c", 'A');
    if (key == -1) {
        perror("Error generating key");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i<20;i++){
        sem_init(&mutex[i],0,1);
    }

   
    while(1) {
        msqid = msgget(key, 0666);
        if (msqid == -1) {
        perror("Error creating/opening message queue");
        exit(EXIT_FAILURE);
        }
        message readRecMessage;
        if (msgrcv(msqid, &readRecMessage, sizeof(readRecMessage.mtext), 102, 0) == -1) 
        {
            perror("Error receiving message from the load balancer");
            exit(EXIT_FAILURE);
        }
        if(strcmp(readRecMessage.mtext,"Terminate") == 0)
        {
            printf("bye");
            exit(0);
        }
        
        
        char graphFileName[50];
        if (sscanf(readRecMessage.mtext, "%d %s %49s", &operation, seqNum ,graphFileName) != 3) 
        {
            fprintf(stderr, "Error parsing the received message\n");
            exit(EXIT_FAILURE);
        }
        pthread_t tid;
        pthread_create(&tid,NULL,makeModify,graphFileName);
        pthread_join(tid,NULL);
    }
    for(int i = 0;i<20;i++)
        sem_destroy(&mutex[i]);
}
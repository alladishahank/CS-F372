#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

struct message
{
    long mtype;
    char mtext[100];
};
typedef struct message message;

typedef struct
{
    pthread_t tid;
} ThreadInfo;

#define MAX_SIZE 30

char graphFileName[50];
int maxNodes;
int adjMatrix[30][30];
int msqid;
char sequenceNumber[4];
int visited[30];
int operation;
int front = -1;
int rear = -1;
int queue[MAX_SIZE];


void * DFS(void * arg);
void * BFS(void * arg);
void *populateMatrix(void *fileName);
void enqueue(int data);
void dequeue();
int queuesize();
void *threadfunc(void *arg);

char result[60];

int main()
{
    key_t key;
    

    key = ftok("load_balancer.c", 'A');
    if (key == -1)
    {
        perror("Error generating key");
        exit(EXIT_FAILURE);
    }

    msqid = msgget(key, 0666);
    if (msqid == -1)
    {
        perror("Error creating/opening message queue");
        exit(EXIT_FAILURE);
    }
    message writeRecMessage;
    int evenodd;
    printf("Is this instance even or odd? Enter 0 for even or 1 for odd \n");
    scanf("%d", &evenodd);
    int whichmtype;
    if (evenodd == 0) whichmtype = 108;
    else whichmtype = 103;
    while (1)
        {
            if (msgrcv(msqid, &writeRecMessage, sizeof(writeRecMessage.mtext), whichmtype, 0) == -1)
            {
                perror("Error receiving message from the load balancer");
                exit(EXIT_FAILURE);
            }
            if (strcmp(writeRecMessage.mtext, "Terminate") == 0)
            {
                printf("bye");
                exit(0);
            }

            if (sscanf(writeRecMessage.mtext, "%d %s %49s", &operation, sequenceNumber, graphFileName) != 3)
            {
                fprintf(stderr, "Error parsing the received message\n");
                exit(EXIT_FAILURE);
            }
            pthread_t tid;
            pthread_create(&tid, NULL, populateMatrix, graphFileName);
            pthread_join(tid, NULL);
        }

        return 0;
    }

void *populateMatrix(void *fileName)
{   
    for(int i = 0;i<30;i++){
        visited[i] = 0;
    }
    key_t newkey = ftok("client.c", 'S');
    int shmid = shmget(newkey, sizeof(int), 0666);
    if (shmid == -1)
    {
        perror("Error getting shared memory");
        exit(EXIT_FAILURE);
    }
    int *sharedData = (int *)shmat(shmid, NULL, 0);
    if ((intptr_t)sharedData == -1)
    {
        perror("Error attaching to shared memory");
        exit(EXIT_FAILURE);
    }
    int* startVer = malloc(sizeof(int*));
    *startVer = (*sharedData) -1;
    if (shmdt(sharedData) == -1)
    {
        perror("Error detaching from shared memory");
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen(fileName, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        exit(0);
    }

    fscanf(file, "%d", &maxNodes);

    for (int i = 0; i < maxNodes; i++)
    {
        for (int j = 0; j < maxNodes; j++)
        {
            fscanf(file, "%d", &adjMatrix[i][j]);
        }
    }
    fclose(file);
    
    pthread_t pid2;
    if (operation == 3) pthread_create(&pid2, NULL, DFS, startVer);
    else if (operation == 4) pthread_create(&pid2, NULL, BFS, startVer);
    pthread_join(pid2, NULL);
    message writeMessage;
    writeMessage.mtype = atoi(sequenceNumber) + 1000;
    strcpy(writeMessage.mtext, result);
    if (msgsnd(msqid, &writeMessage, sizeof(writeMessage.mtext), 0) == -1)
    {
        perror("Error sending message to the client");
        exit(EXIT_FAILURE);
    }
    result[0] = '\0';
    pthread_exit(NULL);
}

void *DFS(void *arg) {
    int i = *((int *)arg);
    free(arg);

    ThreadInfo *info = (ThreadInfo *)malloc(sizeof(ThreadInfo));
    int flag = 1;
    visited[i] = 1;

    for (int j = 0; j < maxNodes; ++j) {
        if (adjMatrix[i][j] && !visited[j]) {
            flag = 0;

            int *start = malloc(sizeof(*start));
            *start = j;

            pthread_create(&info->tid, NULL, DFS, start);
            pthread_join(info->tid, NULL);
        }
    }
    if (flag == 1) {
        char temp[30];
        sprintf(temp, "%d ", i + 1);
        strcat(result, temp);
    }
    free(info);
    
    pthread_exit(NULL);
}
void enqueue(int data) {
    if(front == -1){
        front = 0;
    }
    queue[rear+1] = data;
    rear++;
}
void dequeue(){
    if(front == -1){
        printf("Queue is empty");
    }
    else{
        front++;
    }
}
int queuesize(){
    for(int i=0; i<MAX_SIZE; i++){
        if(queue[i] == -1){
            return i;
        }
    }
    return MAX_SIZE;
}
void *BFS(void *arg) {
    int i = *((int *)arg);
    free(arg);
    

    for(int j=0; j<MAX_SIZE; j++){
        queue[j] = -1;
        visited[j] = 0;
    }
    enqueue(i);
    while(queue[front] != -1) {
        for(int j = front; j <= rear; j++) {
            ThreadInfo *info = (ThreadInfo *)malloc(sizeof(ThreadInfo));
            int *vertex = malloc(sizeof(*vertex));
            *vertex = queue[j];
            pthread_create(&info->tid, NULL, threadfunc, vertex);
            pthread_join(info->tid, NULL);
            free(info);
        }
    }
    for(int i=0; i<queuesize(); i++){
        char temp[30];
        sprintf(temp, "%d ", queue[i]+1);
        strcat(result, temp);
    }
    front = -1;
    rear = -1;
    for(int j=0; j<MAX_SIZE; j++){
        queue[j] = -1;
        visited[j] = 0;
    }
    pthread_exit(NULL);
}

void *threadfunc(void *arg){
    int i = *((int *)arg);
    free(arg);
    for(int k=0; k<maxNodes; k++){
        if(adjMatrix[i][k]==1){
            if(visited[k]==0){
                 enqueue(k);
            }
        }
    }
    dequeue();
    visited[i] = 1;
    pthread_exit(NULL);
}
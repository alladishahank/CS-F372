#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

#define buf 1000*sizeof(int)
#define SHARED_MEM_NAME "shared_mem"
#define SEM_NAME "named_sem"
struct message {
    long mtype;
    char mtext[100];
};
typedef struct message message;

int main() {
    key_t key;
    int msqid;
    sem_t *sem;
    int shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, sizeof(sem_t));
    sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (sem == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    if (sem_init(sem, 1, 1) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
    key = ftok("load_balancer.c", 'A');
    if (key == -1) {
        perror("Error generating key");
        exit(EXIT_FAILURE);
    }

    msqid = msgget(key, 0666);
    if (msqid == -1) {
        perror("Error creating/opening message queue");
        exit(EXIT_FAILURE);
    }
    printf("Message queue created/opened successfully by the client.\n");

    message sendMessage;
    char sequenceNumber[4];
    int operationNumber;
    char *graphFileName = (char *)malloc(50 * sizeof(char));

    while (1) {
        printf("1. Add a new graph to the database\n2. Modify an existing graph of the database\n3. Perform DFS on an existing graph of the database\n4. Perform BFS on an existing graph of the database\n");
        printf("Enter the sequence number: ");
        scanf("%s", sequenceNumber);
        printf("Enter the operation number: ");
        scanf("%d", &operationNumber);
        printf("Enter the graph file name: ");
        scanf("%s", graphFileName);

        char seq[20], op[20];
        snprintf(seq, sizeof(seq), "%s", sequenceNumber);
        snprintf(op, sizeof(op), "%d", operationNumber);
        strcpy(sendMessage.mtext, seq);
        strcat(sendMessage.mtext, " ");
        strcat(sendMessage.mtext, op);
        strcat(sendMessage.mtext, " ");
        strcat(sendMessage.mtext, graphFileName);
        sendMessage.mtype = atoi(sequenceNumber);
        if (operationNumber == 1 || operationNumber == 2) {
            key_t newkey=ftok("client.c", 'B');
            int numNodes;
            printf("Enter number of nodes of the graph: ");
            scanf("%d", &numNodes);
            
            int adjMatrix[numNodes][numNodes];
            printf("Enter the adjacency matrix, each row on a separate line, and elements of a single row separated by whitespace characters:\n");
            for (int i = 0; i < numNodes; i++) {
                for (int j = 0; j < numNodes; j++) {
                    scanf("%d", &adjMatrix[i][j]);
                }
            }
            if (sem_wait(sem) == -1) {
                perror("sem_wait");
                exit(EXIT_FAILURE);
            }
            int shmid = shmget(newkey, buf, IPC_CREAT | 0666);
            if(shmid == -1) {
                perror("Error creating the shared memory");
                exit(-2);
            }
            int *shmptr = (int *)shmat(shmid,NULL,0);
            *shmptr = numNodes;
            shmptr++;
            for (int i = 0; i < numNodes; i++) {
                for (int j = 0; j < numNodes; j++) {
                    *shmptr=adjMatrix[i][j];
                    shmptr++;
                }
            }
            if (msgsnd(msqid, &sendMessage, sizeof(sendMessage.mtext), 0) == -1) {
            printf("Error in sending message to load balancer\n");
            exit(-1);
            }
            
            message receiveSuccessMessage;
          
            if (msgrcv(msqid, &receiveSuccessMessage, sizeof(receiveSuccessMessage.mtext), atoi(sequenceNumber)+1000, 0) == -1) {
                perror("Error receiving message from the primary server");
                exit(EXIT_FAILURE);
            }
            printf("%s",receiveSuccessMessage.mtext);
            shmdt(shmptr);
            if (shmctl(shmid, IPC_RMID, NULL) == -1) 
            {
                perror("Error removing shared memory segment");
                exit(EXIT_FAILURE);
            }
            if (sem_post(sem) == -1) {
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        } 
        else 
        {
            int startVertex;
            printf("Enter the starting vertex: ");
            scanf("%d", &startVertex);
            key_t newkeyS=ftok("client.c", 'S');
            int shmid = shmget(newkeyS, buf, IPC_CREAT | 0666);
            if(shmid == -1) {
                perror("Error creating the shared memory");
                exit(-2);
            }
            int *shmptr = (int *)shmat(shmid,NULL,0);
            *shmptr = startVertex;

            // shared memory is put, now sending message in msgqueue to lb

            if (msgsnd(msqid, &sendMessage, sizeof(sendMessage.mtext), 0) == -1) {
            printf("Error in sending message to load balancer\n");
            exit(-1);
            }
            
            // now we wait for answer from ss

            message receiveSuccessMessage;
          
            if (msgrcv(msqid, &receiveSuccessMessage, sizeof(receiveSuccessMessage.mtext), atoi(sequenceNumber)+1000, 0) == -1) {
                perror("Error receiving message from the primary server");
                exit(EXIT_FAILURE);
            }
            printf("%s\n",receiveSuccessMessage.mtext);
            shmdt(shmptr);
            if (shmctl(shmid, IPC_RMID, NULL) == -1) 
            {
                perror("Error removing shared memory segment");
                exit(EXIT_FAILURE);
            }
        }
    }
    if (sem_destroy(sem) == -1) {
        perror("sem_destroy");
        exit(EXIT_FAILURE);
    }

    // Unmap the shared memory
    if (munmap(sem, sizeof(sem_t)) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    // Close and unlink the shared memory only when the last instance is done
    if (shm_unlink(SHARED_MEM_NAME) == -1) {
        perror("shm_unlink");
        exit(EXIT_FAILURE);
    }
}
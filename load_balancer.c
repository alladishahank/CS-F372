#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define CONTROL_FILE "queue_control_file"
#define PERMISSIONS 0666

struct message
{
    long mtype;
    char mtext[100];
};
typedef struct message message;

int main()
{
    key_t key;
    int msqid;
    message receiveMessage;

        key = ftok("load_balancer.c", 'A');
        if (key == -1)
        {
            perror("Error generating key");
            exit(EXIT_FAILURE);
        }
        msqid = msgget(key, IPC_CREAT | PERMISSIONS);
        if (msqid == -1)
        {
            perror("Error creating message queue");
            exit(EXIT_FAILURE);
        }
        printf("Message queue created successfully by the load balancer\n");


    while (1)
    {

        if (msgrcv(msqid, &receiveMessage, sizeof(receiveMessage.mtext), -101, 0) == -1)
        {
            perror("Error receiving message from the client");
            exit(EXIT_FAILURE);
        }
        // mtype 1 for client sending tuple to the load balancer

        if (receiveMessage.mtype == 101)
        {
            printf("Terminate message received by the load balancer\n");
            message terminateMessagePrimary;
            message terminateMessageSecondary1;
            message terminateMessageSecondary2;
            terminateMessagePrimary.mtype = 102;
            strcpy(terminateMessagePrimary.mtext, "Terminate");
            terminateMessageSecondary1.mtype = 103;
            strcpy(terminateMessageSecondary1.mtext, "Terminate");
            terminateMessageSecondary2.mtype = 108;
            strcpy(terminateMessageSecondary2.mtext, "Terminate");
            if (msgsnd(msqid, &terminateMessagePrimary, sizeof(terminateMessagePrimary.mtext), 0) == -1)
            {
                perror("Error sending message");
                exit(EXIT_FAILURE);
            }
            if (msgsnd(msqid, &terminateMessageSecondary1, sizeof(terminateMessageSecondary1.mtext), 0) == -1)
            {
                perror("Error sending message");
                exit(EXIT_FAILURE);
            }
            if (msgsnd(msqid, &terminateMessageSecondary2, sizeof(terminateMessageSecondary2.mtext), 0) == -1)
            {
                perror("Error sending message");
                exit(EXIT_FAILURE);
            }
            sleep(5);
            msgctl(msqid, IPC_RMID, NULL);
            exit(0);
        }

        char seqNum[4];
        int opNum;
        char graphFileName[50];

        if (sscanf(receiveMessage.mtext, "%s %d %49s", seqNum, &opNum, graphFileName) != 3)
        {
            fprintf(stderr, "Error parsing the received message\n");
            exit(EXIT_FAILURE);
        }
        if (opNum == 1 || opNum == 2)
        {
            // logic for primary server
            message writeMessage;
            writeMessage.mtype = 102;
            snprintf(writeMessage.mtext, sizeof(writeMessage.mtext), "%d", opNum);
            strcat(writeMessage.mtext, " ");
            
            strcat(writeMessage.mtext, seqNum);
            strcat(writeMessage.mtext, " ");
            strcat(writeMessage.mtext, graphFileName);

            if (msgsnd(msqid, &writeMessage, sizeof(writeMessage.mtext), 0) == -1)
            {
                perror("Error sending message to the primary server");
                exit(EXIT_FAILURE);
            }
        }
        else if (opNum == 3 || opNum == 4)
        {
            // logic for secondary server
            message readMessage;
            if(atoi(seqNum) % 2 == 0)
            {
                readMessage.mtype = 108;

            }
            else
            {
                readMessage.mtype = 103;
            }
            snprintf(readMessage.mtext, sizeof(readMessage.mtext), "%d", opNum);
            strcat(readMessage.mtext, " ");
            strcat(readMessage.mtext, seqNum);
            strcat(readMessage.mtext," ");
            strcat(readMessage.mtext, graphFileName);

            if (msgsnd(msqid, &readMessage, sizeof(readMessage.mtext), 0) == -1)
            {
                perror("Error sending message to the secondary server");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            fprintf(stderr, "Invalid operation number");
        }
    }

    return 0;
}
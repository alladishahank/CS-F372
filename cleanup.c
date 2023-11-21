#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct message {
    long mtype;
    char mtext[100];
};
typedef struct message message;

int main() {
    key_t key;
    int msqid;

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
    char isTerminate;
    while(1) {
        printf("Want to terminate the application? Press Y(Yes) or N(No)");
        scanf(" %c",&isTerminate);
        if(isTerminate == 'Y' || isTerminate == 'y') {
            // Send terminate message to load balancer
            message terminateMessage;
            terminateMessage.mtype = 101;
            strcpy(terminateMessage.mtext, "Terminate");
            if (msgsnd(msqid, &terminateMessage, sizeof(terminateMessage.mtext), 0) == -1) {
                perror("Error sending message");
                exit(EXIT_FAILURE);
            }
            exit(0);
        }
    }
}
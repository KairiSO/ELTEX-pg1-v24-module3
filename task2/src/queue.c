#include "queue.h"
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

int queue_create(void)
{
    /* IPC_CREAT | IPC_EXCL — создать и упасть, если уже есть. */
    return msgget(QUEUE_KEY, IPC_CREAT | IPC_EXCL | 0666);
}

int queue_open(void)
{
    return msgget(QUEUE_KEY, 0666);
}

int queue_send(int msqid, long mtype, const char *text)
{
    struct message msg;
    msg.mtype = mtype;
    strncpy(msg.mtext, text, MAX_MSG_SIZE - 1);
    msg.mtext[MAX_MSG_SIZE - 1] = '\0';
    /* Размер данных = длина строки + '\0'. */
    return msgsnd(msqid, &msg, strlen(msg.mtext) + 1, 0);
}

int queue_receive(int msqid, long mtype, struct message *msg, int nowait)
{
    int flags = nowait ? IPC_NOWAIT : 0;
    ssize_t ret = msgrcv(msqid, msg, MAX_MSG_SIZE, mtype, flags);
    if (ret < 0)
        return -1;
    return 0;
}

int queue_remove(int msqid)
{
    return msgctl(msqid, IPC_RMID, NULL);
}

int queue_exists(void)
{
    int id = msgget(QUEUE_KEY, 0666);
    return id >= 0;
}
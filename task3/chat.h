#ifndef CHAT_H
#define CHAT_H

#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MAX_TEXT 256
#define TERM_TYPE 999 /* специальный тип сообщения — сигнал завершения */

struct chat_msg
{
    long mtype;
    char mtext[MAX_TEXT];
};

/* queue.c */
key_t name_to_key(const char *name, int suffix);
int send_msg(int qid, const char *text, long type);
int recv_msg(int qid, struct chat_msg *msg);
void destroy_queue(int qid);

/* chat.c */
void run_chat(int send_qid, int recv_qid, int is_creator,
              int qid1, int qid2);

#endif /* CHAT_H */
#include "chat.h"

#include <stdio.h>
#include <string.h>

/* Преобразуем имя + суффикс в IPC-ключ простым хешем (djb2). */
key_t name_to_key(const char *name, int suffix)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s_%d", name, suffix);

    unsigned long hash = 5381;
    for (const char *p = buf; *p; ++p)
        hash = ((hash << 5) + hash) + (unsigned char)*p;

    return (key_t)(hash & 0x7FFFFFFF);
}

int send_msg(int qid, const char *text, long type)
{
    struct chat_msg m;
    m.mtype = type;
    strncpy(m.mtext, text, MAX_TEXT - 1);
    m.mtext[MAX_TEXT - 1] = '\0';
    return msgsnd(qid, &m, strlen(m.mtext) + 1, 0);
}

int recv_msg(int qid, struct chat_msg *msg)
{
    /* msgtyp = 0 — принимать сообщение любого типа */
    return msgrcv(qid, msg, MAX_TEXT, 0, 0);
}

void destroy_queue(int qid)
{
    msgctl(qid, IPC_RMID, NULL);
}
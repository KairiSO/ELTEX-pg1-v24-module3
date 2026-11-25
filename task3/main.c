#include "chat.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Использование: %s <имя_чата>\n", argv[0]);
        return 1;
    }

    const char *name = argv[1];
    key_t key1 = name_to_key(name, 1);
    key_t key2 = name_to_key(name, 2);

    int qid1, qid2;
    int is_creator;

    /* Пытаемся создать первую очередь эксклюзивно */
    qid1 = msgget(key1, IPC_CREAT | IPC_EXCL | 0666);
    if (qid1 >= 0)
    {
        is_creator = 1;
    }
    else if (errno == EEXIST)
    {
        is_creator = 0;
        qid1 = msgget(key1, 0666);
        if (qid1 < 0)
        {
            perror("msgget");
            return 1;
        }
    }
    else
    {
        perror("msgget");
        return 1;
    }

    /* Создаём/открываем вторую очередь */
    if (is_creator)
    {
        qid2 = msgget(key2, IPC_CREAT | IPC_EXCL | 0666);
        if (qid2 < 0)
        {
            perror("msgget");
            destroy_queue(qid1);
            return 1;
        }
    }
    else
    {
        qid2 = msgget(key2, 0666);
        if (qid2 < 0)
        {
            perror("msgget");
            return 1;
        }
    }

    /*
     * Создатель:
     *   принимает через очередь 1, отправляет через очередь 2.
     * Присоединившийся:
     *   отправляет через очередь 1, принимает через очередь 2.
     */
    int send_qid = is_creator ? qid2 : qid1;
    int recv_qid = is_creator ? qid1 : qid2;

    printf("[%s] роль: %s\n",
           name, is_creator ? "создатель (ждёт подключения)" : "гость");

    run_chat(send_qid, recv_qid, is_creator, qid1, qid2);

    return 0;
}
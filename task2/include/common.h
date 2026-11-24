#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/* Уникальный ключ очереди. Все процессы должны использовать один и тот же. */
#define QUEUE_KEY ((key_t)0x1234ABC0)

/* Максимальная длина текста сообщения (включая завершающий '\0'). */
#define MAX_MSG_SIZE 1024
#define MAX_TOPIC_LEN 64
#define MAX_TOPICS_PER_SUB 16
#define MAX_SUBSCRIBERS 32
#define MAX_PUBLISHERS 32

/* Команды, которые брокер распознаёт в сообщениях с приоритетом 1. */
#define CMD_SUBSCRIBE "subscribe"
#define CMD_UNSUBSCRIBE "unsubscribe"
#define CMD_SEND "send"

/* Структура сообщения очереди System V.
 * mtype — приоритет (1 — управляющее/публикация, pid — для подписчика). */
struct message
{
    long mtype;
    char mtext[MAX_MSG_SIZE];
};

#endif /* COMMON_H */
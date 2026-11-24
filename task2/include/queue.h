#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

/* Создать новую очередь. Возвращает -1, если она уже существует. */
int queue_create(void);

/* Открыть уже существующую очередь. */
int queue_open(void);

/* Отправить сообщение с заданным приоритетом (mtype). */
int queue_send(int msqid, long mtype, const char *text);

/* Получить сообщение. mtype=0 — любое, >0 — строго с таким приоритетом.
 * nowait=1 — не блокироваться, если сообщений нет. */
int queue_receive(int msqid, long mtype, struct message *msg, int nowait);

/* Удалить очередь. */
int queue_remove(int msqid);

/* Проверить, существует ли очередь. */
int queue_exists(void);

#endif /* QUEUE_H */
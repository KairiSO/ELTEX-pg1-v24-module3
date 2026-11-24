#include "subscriber.h"
#include "queue.h"
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    running = 0;
}

void subscriber_run(int topic_count, char **topics)
{
    int msqid = queue_open();
    if (msqid < 0)
    {
        perror("[subscriber] queue_open (брокер запущен?)");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    pid_t pid = getpid();
    printf("[subscriber] запущен, pid=%d\n", pid);

    /* Подписываемся на все темы из аргументов. */
    for (int i = 0; i < topic_count && running; i++)
    {
        char buf[MAX_MSG_SIZE];
        snprintf(buf, sizeof(buf), "%s,%d,%s",
                 CMD_SUBSCRIBE, pid, topics[i]);
        if (queue_send(msqid, 1, buf) < 0)
        {
            perror("[subscriber] subscribe");
            exit(1);
        }
        printf("[subscriber] подписка на '%s'\n", topics[i]);
    }

    /* Основной цикл: читаем сообщения, адресованные именно нам (mtype == pid). */
    while (running)
    {
        struct message msg;
        if (queue_receive(msqid, pid, &msg, 1) == 0)
        {
            printf("[subscriber] получено: %s\n", msg.mtext);
        }
        else
        {
            if (errno == EIDRM)
            {
                printf("[subscriber] очередь удалена, выход.\n");
                break;
            }
            if (errno == ENOMSG)
                usleep(50000);
        }
    }

    /* Отписываемся от всех тем (на случай, если брокер ещё жив). */
    for (int i = 0; i < topic_count; i++)
    {
        char buf[MAX_MSG_SIZE];
        snprintf(buf, sizeof(buf), "%s,%d,%s",
                 CMD_UNSUBSCRIBE, pid, topics[i]);
        queue_send(msqid, 1, buf); /* ошибка здесь не критична */
    }

    printf("[subscriber] выход.\n");
}
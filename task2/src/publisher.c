#include "publisher.h"
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

void publisher_run(const char *topic)
{
    int msqid = queue_open();
    if (msqid < 0)
    {
        perror("[publisher] queue_open (брокер запущен?)");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    pid_t pid = getpid();
    printf("[publisher] запущен, pid=%d, topic='%s'\n", pid, topic);
    printf("[publisher] вводите сообщения (Ctrl+D для выхода):\n");

    char line[MAX_MSG_SIZE];
    int counter = 1;
    while (running && fgets(line, sizeof(line), stdin))
    {
        line[strcspn(line, "\n")] = '\0';

        char buf[MAX_MSG_SIZE];
        /* Формат: "send,<pid>,<topic>,<payload>" */
        snprintf(buf, sizeof(buf), "%s,%d,%s,%s",
                 CMD_SEND, pid, topic, line);

        if (queue_send(msqid, 1, buf) < 0)
        {
            if (errno == EIDRM)
            {
                printf("[publisher] очередь удалена, выход.\n");
                break;
            }
            perror("[publisher] queue_send");
            break;
        }
        printf("[publisher] отправлено сообщение #%d\n", counter++);
    }

    printf("[publisher] выход.\n");
}
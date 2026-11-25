#include "chat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

/* Глобальные переменные, доступные обработчику сигнала */
static volatile sig_atomic_t g_got_sigint = 0;
static int g_send_qid = -1;

static void sigint_handler(int sig)
{
    (void)sig;
    g_got_sigint = 1;
    /* Отправляем сообщение с «известным приоритетом» (типом) TERM_TYPE */
    if (g_send_qid >= 0)
        send_msg(g_send_qid, "", TERM_TYPE);
}

/* Цикл приёма: ребёнок читает очередь и печатает сообщения */
static void receiver_loop(int recv_qid, int is_creator)
{
    struct chat_msg msg;

    if (is_creator)
        printf("[ожидание подключения второго пользователя...]\n");

    while (1)
    {
        if (recv_msg(recv_qid, &msg) < 0)
        {
            if (errno == EINTR)
                continue; /* прервано сигналом — повторим */
            break;
        }

        if (msg.mtype == TERM_TYPE)
        {
            printf("\n[собеседник завершил работу]\n");
            break;
        }

        printf("собеседник: %s\n", msg.mtext);
        fflush(stdout);
    }
}

/* Цикл отправки: родитель читает stdin и кладёт в очередь */
static void sender_loop(int send_qid)
{
    char buf[MAX_TEXT];

    printf("[вы подключены. Введите сообщение (Ctrl+C — выход)]:\n");
    fflush(stdout);

    while (!g_got_sigint)
    {
        if (!fgets(buf, sizeof(buf), stdin))
        {
            if (g_got_sigint)
                break;
            break; /* EOF */
        }

        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] == '\0')
            continue;

        if (send_msg(send_qid, buf, 1) < 0)
        {
            perror("msgsnd");
            break;
        }
    }
}

void run_chat(int send_qid, int recv_qid, int is_creator,
              int qid1, int qid2)
{
    g_send_qid = send_qid;

    /* Устанавливаем обработчик SIGINT */
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return;
    }

    if (pid == 0)
    {
        /* ---- РЕБЁНОК: приём ---- */
        /* Игнорируем SIGINT в ребёнке, чтобы завершение инициировал родитель */
        signal(SIGINT, SIG_IGN);
        receiver_loop(recv_qid, is_creator);
        _exit(0);
    }

    /* ---- РОДИТЕЛЬ: отправка ---- */
    sender_loop(send_qid);

    /* Если вышли не по SIGINT (например, EOF), всё равно шлём TERM */
    if (!g_got_sigint)
        send_msg(send_qid, "", TERM_TYPE);

    /* Ждём завершения ребёнка */
    waitpid(pid, NULL, 0);

    /* Создатель удаляет очереди */
    if (is_creator)
    {
        destroy_queue(qid1);
        destroy_queue(qid2);
        printf("[очереди удалены]\n");
    }
}
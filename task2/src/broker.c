#include "broker.h"
#include "queue.h"
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Подписчик: его pid и список тем, на которые он подписан. */
typedef struct
{
    pid_t pid;
    char topics[MAX_TOPICS_PER_SUB][MAX_TOPIC_LEN];
    int topic_count;
} subscriber_t;

/* Состояние брокера. */
typedef struct
{
    subscriber_t subs[MAX_SUBSCRIBERS];
    int sub_count;
    pid_t publishers[MAX_PUBLISHERS];
    int pub_count;
    int msqid;
    volatile sig_atomic_t running;
} broker_state_t;

static broker_state_t state;

/* Обработчик SIGINT — просто выставляет флаг. */
static void sigint_handler(int sig)
{
    (void)sig;
    state.running = 0;
}

/* ---------- Работа со списком подписчиков ---------- */

static subscriber_t *find_subscriber(pid_t pid)
{
    for (int i = 0; i < state.sub_count; i++)
        if (state.subs[i].pid == pid)
            return &state.subs[i];
    return NULL;
}

static subscriber_t *add_subscriber(pid_t pid)
{
    if (state.sub_count >= MAX_SUBSCRIBERS)
        return NULL;
    subscriber_t *s = &state.subs[state.sub_count++];
    s->pid = pid;
    s->topic_count = 0;
    return s;
}

static void remove_subscriber(pid_t pid)
{
    for (int i = 0; i < state.sub_count; i++)
    {
        if (state.subs[i].pid == pid)
        {
            state.subs[i] = state.subs[--state.sub_count];
            return;
        }
    }
}

static void add_topic(subscriber_t *s, const char *topic)
{
    for (int i = 0; i < s->topic_count; i++)
        if (strcmp(s->topics[i], topic) == 0)
            return; /* уже есть */
    if (s->topic_count < MAX_TOPICS_PER_SUB)
    {
        strncpy(s->topics[s->topic_count++], topic, MAX_TOPIC_LEN - 1);
        s->topics[s->topic_count - 1][MAX_TOPIC_LEN - 1] = '\0';
    }
}

static void remove_topic(subscriber_t *s, const char *topic)
{
    for (int i = 0; i < s->topic_count; i++) {
        if (strcmp(s->topics[i], topic) == 0) {
            strcpy(s->topics[i], s->topics[--s->topic_count]);
            return;
        }
    }
}

/* ---------- Работа со списком издателей ---------- */

static void add_publisher(pid_t pid)
{
    for (int i = 0; i < state.pub_count; i++)
        if (state.publishers[i] == pid)
            return;
    if (state.pub_count < MAX_PUBLISHERS)
        state.publishers[state.pub_count++] = pid;
}

/* ---------- Рассылка ---------- */

static void forward_to_subscribers(const char *topic, const char *payload)
{
    for (int i = 0; i < state.sub_count; i++)
    {
        subscriber_t *s = &state.subs[i];
        for (int j = 0; j < s->topic_count; j++)
        {
            if (strcmp(s->topics[j], topic) == 0)
            {
                char buf[MAX_MSG_SIZE];
                snprintf(buf, sizeof(buf), "%s:%s", topic, payload);
                /* mtype = pid подписчика — он заберёт именно это сообщение. */
                queue_send(state.msqid, s->pid, buf);
                break;
            }
        }
    }
}

/* ---------- Разбор входящего управляющего сообщения ---------- */

static void handle_message(const struct message *msg)
{
    char text[MAX_MSG_SIZE];
    strncpy(text, msg->mtext, MAX_MSG_SIZE - 1);
    text[MAX_MSG_SIZE - 1] = '\0';

    char *cmd = strtok(text, ",");
    if (!cmd)
        return;

    if (strcmp(cmd, CMD_SUBSCRIBE) == 0)
    {
        char *pid_str = strtok(NULL, ",");
        char *topic = strtok(NULL, ",");
        if (!pid_str || !topic)
            return;
        pid_t pid = (pid_t)atoi(pid_str);
        subscriber_t *s = find_subscriber(pid);
        if (!s)
            s = add_subscriber(pid);
        if (s)
            add_topic(s, topic);
        printf("[broker] subscriber %d subscribed to '%s'\n", pid, topic);
    }
    else if (strcmp(cmd, CMD_UNSUBSCRIBE) == 0)
    {
        char *pid_str = strtok(NULL, ",");
        char *topic = strtok(NULL, ",");
        if (!pid_str || !topic)
            return;
        pid_t pid = (pid_t)atoi(pid_str);
        subscriber_t *s = find_subscriber(pid);
        if (s)
        {
            remove_topic(s, topic);
            if (s->topic_count == 0)
                remove_subscriber(pid);
        }
        printf("[broker] subscriber %d unsubscribed from '%s'\n", pid, topic);
    }
    else if (strcmp(cmd, CMD_SEND) == 0)
    {
        char *pid_str = strtok(NULL, ",");
        char *topic = strtok(NULL, ",");
        if (!pid_str || !topic)
            return;
        pid_t pid = (pid_t)atoi(pid_str);
        add_publisher(pid);
        /* Полезная нагрузка — всё, что идёт после третьей запятой. */
        char *payload = topic + strlen(topic) + 1;
        printf("[broker] publisher %d -> topic '%s': %s\n", pid, topic, payload);
        forward_to_subscribers(topic, payload);
    }
}

/* ---------- Главная функция брокера ---------- */

void broker_run(void)
{
    if (queue_exists())
    {
        fprintf(stderr,
                "[broker] очередь уже существует (брокер уже запущен)\n");
        exit(1);
    }

    state.msqid = queue_create();
    if (state.msqid < 0)
    {
        perror("[broker] queue_create");
        exit(1);
    }

    state.sub_count = 0;
    state.pub_count = 0;
    state.running = 1;

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("[broker] запущен, msqid=%d, pid=%d\n", state.msqid, getpid());

    /* Основной цикл: читаем управляющие сообщения (mtype == 1). */
    while (state.running)
    {
        struct message msg;
        if (queue_receive(state.msqid, 1, &msg, 1) == 0)
        {
            handle_message(&msg);
        }
        else
        {
            if (errno == ENOMSG)
            {
                usleep(50000); /* 50 мс — нет сообщений */
            }
            else if (errno == EIDRM)
            {
                break; /* очередь удалена извне */
            }
            else
            {
                usleep(50000);
            }
        }
    }

    printf("[broker] завершение, рассылаем SIGINT клиентам...\n");

    for (int i = 0; i < state.pub_count; i++)
        kill(state.publishers[i], SIGINT);
    for (int i = 0; i < state.sub_count; i++)
        kill(state.subs[i].pid, SIGINT);

    /* Ждём, пока очередь опустеет, но не дольше 3 секунд. */
    time_t start = time(NULL);
    while (time(NULL) - start < 3)
    {
        struct message msg;
        /* mtype = 0 — принять сообщение любого типа. */
        if (msgrcv(state.msqid, &msg, MAX_MSG_SIZE, 0, IPC_NOWAIT) < 0)
        {
            if (errno == ENOMSG || errno == EIDRM)
                break;
        }
    }

    queue_remove(state.msqid);
    printf("[broker] очередь удалена, выход.\n");
}
/*
Задание 6 (UDP сокеты)
Реализовать групповой чат с использованием широковещательной
рассылки через UDP-сокет. Клиенты равноправные (одно приложение).
При подключении (запуске программы) клиент рассылает
широковещательное сообщение. Другие подключенные клиенты отображают
сообщение, что в сети появился новый участник. Далее сообщения,
отправляемые клиентами, принимают остальные клиенты. При отключении
приложение отправляет сообщение, что оно не в сети.
Рекомендуется пересылать сообщения по физической сети, или
виртуальной сети между виртуальными машинами.
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/select.h>


#define CHAT_PORT 12345
#define BROADCAST_ADDR "255.255.255.255"
#define BUF_SIZE 1024

//special msg_prefixes
#define JOIN_PREFIX "[JOIN]"
#define LEAVE_PREFIX "[LEAVE]"
#define CHAT_PREFIX ""

static int g_sockfd = -1;
static const char *g_username = NULL;

static volatile sig_atomic_t g_running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

//отправка широковещательного сообщения всем клиентам в сети
static int broadcast_message(const char *message)
{
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
    broadcast_addr.sin_port = htons(CHAT_PORT);

    if (sendto(g_sockfd, message, strlen(message), 0,
               (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0)
    {
        perror("sendto");
        return -1;
    }
    return 0;
}
// 1. Создаётся структура sockaddr_in для широковещательного адреса.
// 2. Указываются семейство адресов AF_INET, широковещательный IP и порт.
// 3. Отправляется сообщение через UDP-сокет всем клиентам в сети.
// 4. Если отправка не удалась, выводится ошибка и возвращается -1.
// 5. В случае успеха возвращается 0.
static void send_service_message(const char *type)
{
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "%s%s", type, g_username);
    broadcast_message(buf);
}

static void send_chat_message(const char *message)
{
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "%s%s: %s", CHAT_PREFIX, g_username, message);
    broadcast_message(buf);
}

//create and manage UDP socket for chat communication
static int create_udp_socket(void)
{
    int fd  = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        perror("socket");
        return -1;
    }
    
    int broadcast = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
    {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(CHAT_PORT);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(fd);
        return -1;
    }
    return fd;
}

/*
Основной цикл приложения:
 - читаем свои сообщения из stdin
 - принимаем чужие сообщения из сокета
Используется select(), чтобы не блокироваться на одном источнике.*/

static void chat_loop(void)
{
    char inbuf[BUF_SIZE];

    while (g_running)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int ready = select(g_sockfd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0)
        {
            perror("select");
            break;
        }

        if (FD_ISSET(g_sockfd, &readfds))
        {
            ssize_t len = recv(g_sockfd, inbuf, sizeof(inbuf), 0);
            if (len > 0)
            {
                inbuf[len] = '\0';
                printf("%s\n", inbuf);
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            if (fgets(inbuf, sizeof(inbuf), stdin) != NULL)
            {
                size_t len = strlen(inbuf);
                if (len > 0 && inbuf[len - 1] == '\n')
                {
                    inbuf[len - 1] = '\0';
                }
                send_chat_message(inbuf);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <username>\n", argv[0]);
        return 1;
    }
    g_username = argv[1];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    g_sockfd = create_udp_socket();
    if (g_sockfd < 0)
    {
        return 1;
    }
    printf("Chat is started, Name: %s, Port: %d\n", g_username, CHAT_PORT);
    printf("For exit, press Ctrl+C\n");
    fflush(stdout);

    send_service_message(JOIN_PREFIX);

    chat_loop();

    send_service_message(LEAVE_PREFIX);

    close(g_sockfd);
    printf("Chat is ended\n");
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#define PORT 12345
#define MAX_CLIENTS 10
#define BUF_SIZE 4096

int clients[MAX_CLIENTS];
int num_clients = 0;

void broadcast(int exclude_fd, const char *msg, int len)
{
    for (int i = 0; i < num_clients; i++)
    {
        if (clients[i] != exclude_fd)
        {
            send(clients[i], msg, len, MSG_NOSIGNAL);
        }
    }
}

int main()
{
    signal(SIGPIPE, SIG_IGN);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }
    listen(listen_fd, 5);

    struct pollfd fds[MAX_CLIENTS + 1];
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;
    int nfds = 1;

    printf("Server started on port %d\n", PORT);

    while (1)
    {
        poll(fds, nfds, -1);

        for (int i = 0; i < nfds; i++)
        {
            if (!(fds[i].revents & POLLIN))
                continue;

            if (fds[i].fd == listen_fd)
            {
                int cfd = accept(listen_fd, NULL, NULL);
                if (cfd < 0 || num_clients >= MAX_CLIENTS)
                {
                    if (cfd >= 0)
                        close(cfd);
                    continue;
                }
                clients[num_clients++] = cfd;
                fds[nfds].fd = cfd;
                fds[nfds].events = POLLIN;
                nfds++;
                printf("Client connected (fd=%d, total=%d)\n", cfd, num_clients);
            }
            else
            {
                char buf[BUF_SIZE];
                int len = recv(fds[i].fd, buf, sizeof(buf), 0);
                if (len <= 0)
                {
                    printf("Client disconnected (fd=%d)\n", fds[i].fd);
                    close(fds[i].fd);
                    for (int j = 0; j < num_clients; j++)
                    {
                        if (clients[j] == fds[i].fd)
                        {
                            clients[j] = clients[--num_clients];
                            break;
                        }
                    }
                    fds[i] = fds[--nfds];
                    i--;
                }
                else
                {
                    broadcast(fds[i].fd, buf, len);
                }
            }
        }
    }
    return 0;
}
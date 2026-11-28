#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>

#define PORT 12345
#define SERVER "127.0.0.1"
#define BUF_SIZE 4096

int sockfd = -1;
const char *username;
int running = 1;

void sigint_handler(int sig)
{
    (void)sig;
    running = 0;
}

void send_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        perror("fopen");
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;

    char header[256];
    int hlen = snprintf(header, sizeof(header), "[FILE] %s %s %ld\n", username, name, size);
    send(sockfd, header, hlen, 0);

    char buf[1024];
    size_t read;
    while ((read = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        send(sockfd, buf, read, 0);
    }
    fclose(f);
    printf("Sent file: %s (%ld bytes)\n", name, size);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <username>\n", argv[0]);
        return 1;
    }
    username = argv[1];

    signal(SIGINT, sigint_handler);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        fprintf(stderr, "Make sure server is running on %s:%d\n", SERVER, PORT);
        return 1;
    }

    printf("Connected to server as %s\n", username);

    char join_msg[256];
    int join_len = snprintf(join_msg, sizeof(join_msg), "[JOIN] %s\n", username);
    send(sockfd, join_msg, join_len, 0);

    char buf[BUF_SIZE];
    int file_mode = 0;
    FILE *file_out = NULL;
    long file_size = 0;
    long file_received = 0;
    char file_name[256] = {0};

    while (running)
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(sockfd, &rfds);
        int maxfd = STDIN_FILENO > sockfd ? STDIN_FILENO : sockfd;

        select(maxfd + 1, &rfds, NULL, NULL, NULL);

        if (FD_ISSET(sockfd, &rfds))
        {
            int len = recv(sockfd, buf, sizeof(buf), 0);
            if (len <= 0)
            {
                printf("Server disconnected\n");
                break;
            }

            if (file_mode)
            {
                fwrite(buf, 1, len, file_out);
                file_received += len;
                if (file_received >= file_size)
                {
                    fclose(file_out);
                    printf("\n[Received file: %s (%ld bytes)]\n> ", file_name, file_size);
                    fflush(stdout);
                    file_mode = 0;
                }
            }
            else
            {
                buf[len] = 0;

                if (strncmp(buf, "[FILE] ", 7) == 0)
                {
                    char from[64], fname[256];
                    if (sscanf(buf, "[FILE] %s %s %ld", from, fname, &file_size) == 3)
                    {
                        snprintf(file_name, sizeof(file_name), "%s_%s", from, fname);
                        file_out = fopen(file_name, "wb");
                        if (file_out)
                        {
                            file_mode = 1;
                            file_received = 0;
                            char *nl = strchr(buf, '\n');
                            if (nl)
                            {
                                int header_len = nl - buf + 1;
                                int data_len = len - header_len;
                                if (data_len > 0)
                                {
                                    fwrite(buf + header_len, 1, data_len, file_out);
                                    file_received += data_len;
                                }
                                if (file_received >= file_size)
                                {
                                    fclose(file_out);
                                    printf("\n[Received file: %s (%ld bytes)]\n> ", file_name, file_size);
                                    fflush(stdout);
                                    file_mode = 0;
                                }
                            }
                        }
                    }
                }
                else
                {
                    printf("\r%s", buf);
                    fflush(stdout);
                }
            }
        }

        if (FD_ISSET(STDIN_FILENO, &rfds))
        {
            char line[1024];
            if (!fgets(line, sizeof(line), stdin))
            {
                running = 0;
                break;
            }
            int len = strlen(line);
            if (len > 0 && line[len - 1] == '\n')
                line[--len] = 0;
            if (len == 0)
                continue;

            if (strncmp(line, "/send ", 6) == 0)
            {
                send_file(line + 6);
            }
            else
            {
                char msg[1024];
                int mlen = snprintf(msg, sizeof(msg), "%s: %s\n", username, line);
                send(sockfd, msg, mlen, 0);
            }
        }
    }

    char leave_msg[256];
    int leave_len = snprintf(leave_msg, sizeof(leave_msg), "[LEAVE] %s\n", username);
    send(sockfd, leave_msg, leave_len, 0);
    close(sockfd);
    printf("\nDisconnected\n");
    return 0;
}
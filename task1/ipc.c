/* ipc.c - Реализация IPC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "ipc.h"

int init_channels(const char *base_name, ChannelHandles *h)
{
    if (base_name == NULL)
    {
        int fds1[2], fds2[2];
        if (pipe(fds1) == -1)
        {
            perror("pipe 1");
            return -1;
        }
        if (pipe(fds2) == -1)
        {
            perror("pipe 2");
            close(fds1[0]);
            close(fds1[1]);
            return -1;
        }

        h->p2c_rfd = fds1[0]; /* дочерний читает */
        h->p2c_wfd = fds1[1]; /* родитель пишет */
        h->c2p_rfd = fds2[0]; /* родитель читает */
        h->c2p_wfd = fds2[1]; /* дочерний пишет */
    }
    else
    {
        char fin[512], fout[512];
        snprintf(fin, sizeof(fin), "%s.in", base_name);
        snprintf(fout, sizeof(fout), "%s.out", base_name);

        struct stat st;
        if (stat(fin, &st) != 0 || !S_ISFIFO(st.st_mode))
            if (mkfifo(fin, 0666) == -1 && errno != EEXIST)
            {
                perror("mkfifo in");
                return -1;
            }
        if (stat(fout, &st) != 0 || !S_ISFIFO(st.st_mode))
            if (mkfifo(fout, 0666) == -1 && errno != EEXIST)
            {
                perror("mkfifo out");
                unlink(fin);
                return -1;
            }

        /* Для именованных каналов дескрипторы открываются ПОСЛЕ fork,
           чтобы избежать deadlock при ожидании второй стороны. */
        h->p2c_rfd = h->p2c_wfd = h->c2p_rfd = h->c2p_wfd = -1;
    }
    return 0;
}

void close_unused_ends(ChannelHandles *h, int is_parent)
{
    if (is_parent)
    {
        if (h->p2c_rfd != -1)
        {
            close(h->p2c_rfd);
            h->p2c_rfd = -1;
        }
        if (h->c2p_wfd != -1)
        {
            close(h->c2p_wfd);
            h->c2p_wfd = -1;
        }
    }
    else
    {
        if (h->p2c_wfd != -1)
        {
            close(h->p2c_wfd);
            h->p2c_wfd = -1;
        }
        if (h->c2p_rfd != -1)
        {
            close(h->c2p_rfd);
            h->c2p_rfd = -1;
        }
    }
}

void close_all_channels(ChannelHandles *h)
{
    if (h->p2c_rfd != -1)
        close(h->p2c_rfd);
    if (h->p2c_wfd != -1)
        close(h->p2c_wfd);
    if (h->c2p_rfd != -1)
        close(h->c2p_rfd);
    if (h->c2p_wfd != -1)
        close(h->c2p_wfd);
}

int send_message(int fd, MessageType type, const void *payload, size_t len)
{
    MsgHeader hdr = {.type = type, .payload_len = len};
    if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
    {
        perror("write header");
        return -1;
    }
    if (len > 0 && payload)
    {
        if (write(fd, payload, len) != (ssize_t)len)
        {
            perror("write payload");
            return -1;
        }
    }
    return 0;
}

MessageType receive_message(int fd, MsgHeader *hdr_out, void *buf, size_t buf_size)
{
    ssize_t n = read(fd, hdr_out, sizeof(MsgHeader));
    if (n != sizeof(MsgHeader))
    {
        if (n == 0)
            return MSG_ERROR;
        perror("read header");
        return MSG_ERROR;
    }
    if (hdr_out->payload_len > 0)
    {
        if (hdr_out->payload_len > buf_size)
        {
            fprintf(stderr, "buffer too small\n");
            return MSG_ERROR;
        }
        if (read(fd, buf, hdr_out->payload_len) != (ssize_t)hdr_out->payload_len)
        {
            perror("read payload");
            return MSG_ERROR;
        }
    }
    return hdr_out->type;
}
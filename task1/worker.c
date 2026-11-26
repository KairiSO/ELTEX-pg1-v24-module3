/* worker.c - Реализация логики процессов */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "worker.h"

ssize_t get_file_size(const char *filename)
{
    struct stat st;
    return stat(filename, &st) == 0 ? st.st_size : -1;
}

int file_exists(const char *filename)
{
    return access(filename, F_OK) != -1;
}

/* Отправить содержимое файла блоками */
static int send_file_data(int fd, const char *filename)
{
    int in_fd = open(filename, O_RDONLY);
    if (in_fd == -1)
    {
        perror("open input");
        return -1;
    }

    char buf[BLOCK_SIZE];
    ssize_t n;
    while ((n = read(in_fd, buf, BLOCK_SIZE)) > 0)
        if (send_message(fd, MSG_DATA_BLOCK, buf, n) != 0)
        {
            close(in_fd);
            return -1;
        }

    if (n == -1)
    {
        perror("read file");
        close(in_fd);
        return -1;
    }
    close(in_fd);
    return send_message(fd, MSG_FILE_DONE, NULL, 0);
}

int run_parent(ChannelHandles *h, char **filenames, int num_files)
{
    printf("[parent] Waiting for child...\n");

    MsgHeader hdr;
    char tmp[MAX_FILENAME_LEN + sizeof(size_t)];
    if (receive_message(h->c2p_rfd, &hdr, tmp, sizeof(tmp)) != MSG_READY)
    {
        fprintf(stderr, "[parent] Child didn't send READY\n");
        return -1;
    }
    printf("[parent] Child is ready\n");

    for (int i = 0; i < num_files; i++)
    {
        const char *fname = filenames[i];

        if (!file_exists(fname))
        {
            fprintf(stderr, "[parent] File not found: %s\n", fname);
            continue;
        }
        
        ssize_t fsize = get_file_size(fname);
        if (fsize < 0)
        {
            fprintf(stderr, "[parent] Can't stat: %s\n", fname);
            continue;
        }

        printf("[parent] Sending: %s (%zd bytes)\n", fname, fsize);

        FileInfoPayload info;
        strncpy(info.filename, fname, MAX_FILENAME_LEN - 1);
        info.filename[MAX_FILENAME_LEN - 1] = '\0';
        info.file_size = (size_t)fsize;

        if (send_message(h->p2c_wfd, MSG_FILE_INFO, &info, sizeof(info)) != 0)
            return -1;
        if (send_file_data(h->p2c_wfd, fname) != 0)
            return -1;
    }

    printf("[parent] All files sent, notifying child\n");
    return send_message(h->p2c_wfd, MSG_ALL_DONE, NULL, 0);
}

int run_child(ChannelHandles *h)
{
    printf("[child] Started\n");
    if (send_message(h->c2p_wfd, MSG_READY, NULL, 0) != 0)
        return -1;

    int out_fd = -1;
    char cur_name[MAX_FILENAME_LEN] = "";
    size_t expected = 0, received = 0;

    while (1)
    {
        MsgHeader hdr;
        size_t buf_size = sizeof(FileInfoPayload) > BLOCK_SIZE ? sizeof(FileInfoPayload) : BLOCK_SIZE;
        void *buf = malloc(buf_size);
        if (!buf)
        {
            perror("malloc");
            return -1;
        }

        MessageType type = receive_message(h->p2c_rfd, &hdr, buf, buf_size);

        switch (type)
        {
        case MSG_FILE_INFO:
        {
            FileInfoPayload *info = buf;
            strncpy(cur_name, info->filename, MAX_FILENAME_LEN - 1);
            cur_name[MAX_FILENAME_LEN - 1] = '\0';
            expected = info->file_size;
            received = 0;

            char copy[MAX_FILENAME_LEN + 10];
            snprintf(copy, sizeof(copy), "%s.copy", cur_name);

            if (out_fd != -1)
                close(out_fd);
            out_fd = open(copy, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd == -1)
            {
                perror("open output");
                free(buf);
                send_message(h->c2p_wfd, MSG_ERROR, NULL, 0);
                return -1;
            }
            printf("[child] Receiving: %s -> %s (%zu bytes)\n", cur_name, copy, expected);
            break;
        }
        case MSG_DATA_BLOCK:
        {
            if (out_fd == -1)
            {
                fprintf(stderr, "[child] data without file info\n");
                free(buf);
                return -1;
            }
            ssize_t w = write(out_fd, buf, hdr.payload_len);
            if (w != (ssize_t)hdr.payload_len)
            {
                perror("write");
                free(buf);
                return -1;
            }
            received += w;
            break;
        }
        case MSG_FILE_DONE:
            if (out_fd != -1)
            {
                close(out_fd);
                out_fd = -1;
            }
            printf("[child] Done %s (%zu/%zu bytes)\n", cur_name, received, expected);
            break;
        case MSG_ALL_DONE:
            printf("[child] All done, exiting\n");
            if (out_fd != -1)
                close(out_fd);
            free(buf);
            return 0;
        case MSG_ERROR:
            fprintf(stderr, "[child] Error from parent\n");
            if (out_fd != -1)
                close(out_fd);
            free(buf);
            return -1;
        default:
            fprintf(stderr, "[child] Unknown msg %d\n", type);
            if (out_fd != -1)
                close(out_fd);
            free(buf);
            return -1;
        }
        free(buf);
    }
}
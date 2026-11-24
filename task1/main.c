/* main.c - Точка входа */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include "ipc.h"
#include "worker.h"

static void usage(const char *p)
{
    fprintf(stderr, "Usage: %s [-p pipe_name] file1 [file2 ...]\n", p);
}

int main(int argc, char **argv)
{
    const char *pipe_name = NULL;
    int start = 1;

    if (argc > 1 && strcmp(argv[1], "-p") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "-p requires argument\n");
            usage(argv[0]);
            return 1;
        }
        pipe_name = argv[2];
        start = 3;
    }

    int num_files = argc - start;
    if (num_files <= 0)
    {
        fprintf(stderr, "No files\n");
        usage(argv[0]);
        return 1;
    }

    char **filenames = &argv[start];

    ChannelHandles h;
    if (init_channels(pipe_name, &h) != 0)
        return 1;

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close_all_channels(&h);
        return 1;
    }

    if (pid == 0)
    {
        /* Дочерний */
        if (pipe_name)
        {
            char fin[512], fout[512];
            snprintf(fin, sizeof(fin), "%s.in", pipe_name);
            snprintf(fout, sizeof(fout), "%s.out", pipe_name);
            h.p2c_rfd = open(fin, O_RDONLY);
            h.c2p_wfd = open(fout, O_WRONLY);
            if (h.p2c_rfd == -1 || h.c2p_wfd == -1)
            {
                perror("child open");
                exit(1);
            }
        }
        else
        {
            close_unused_ends(&h, 0);
        }
        int rc = run_child(&h);
        close_all_channels(&h);
        exit(rc);
    }

    /* Родитель */
    if (pipe_name)
    {
        char fin[512], fout[512];
        snprintf(fin, sizeof(fin), "%s.in", pipe_name);
        snprintf(fout, sizeof(fout), "%s.out", pipe_name);
        h.p2c_wfd = open(fin, O_WRONLY);
        h.c2p_rfd = open(fout, O_RDONLY);
        if (h.p2c_wfd == -1 || h.c2p_rfd == -1)
        {
            perror("parent open");
            kill(pid, SIGTERM);
            wait(NULL);
            unlink(fin);
            unlink(fout);
            return 1;
        }
    }
    else
    {
        close_unused_ends(&h, 1);
    }

    int rc = run_parent(&h, filenames, num_files);

    int status;
    waitpid(pid, &status, 0);
    close_all_channels(&h);

    if (pipe_name)
    {
        char fin[512], fout[512];
        snprintf(fin, sizeof(fin), "%s.in", pipe_name);
        snprintf(fout, sizeof(fout), "%s.out", pipe_name);
        unlink(fin);
        unlink(fout);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
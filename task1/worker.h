/* worker.h - Логика процессов и работа с файлами */
#ifndef WORKER_H
#define WORKER_H

#include "ipc.h"
#include <sys/types.h>

ssize_t get_file_size(const char *filename);
int file_exists(const char *filename);

int run_parent(ChannelHandles *h, char **filenames, int num_files);
int run_child(ChannelHandles *h);

#endif
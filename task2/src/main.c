#include "common.h"
#include "broker.h"
#include "publisher.h"
#include "subscriber.h"
#include <stdio.h>
#include <string.h>


static void usage(const char *prog)
{
    fprintf(stderr, "Использование:\n");
    fprintf(stderr, "  %s -b                        — брокер\n", prog);
    fprintf(stderr, "  %s -p <topic>                — издатель\n", prog);
    fprintf(stderr, "  %s -s <topic1> [topic2 ...]  — подписчик\n", prog);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0)
    {
        broker_run();
    }
    else if (strcmp(argv[1], "-p") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "издатель: требуется аргумент <topic>\n");
            return 1;
        }
        publisher_run(argv[2]);
    }
    else if (strcmp(argv[1], "-s") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "подписчик: требуется хотя бы одна тема\n");
            return 1;
        }
        subscriber_run(argc - 2, argv + 2);
    }
    else
    {
        usage(argv[0]);
        return 1;
    }

    return 0;
}
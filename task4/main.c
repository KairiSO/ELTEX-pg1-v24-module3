#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include <time.h>

#define SHM_SIZE 4096
#define MAX_ELEMENTS 10
#define SEM_NAME "/mysemaphore"
#define SHM_NAME "/mysharedmemory"

typedef struct
{
    int first_block_offset;
    int last_block_offset;
    int producer_finished;
    int free_block_offset;
} ShmHeader;

typedef struct
{
    int count;
    int next_block_offset;
    int data[MAX_ELEMENTS];
} ShmDataBlock;

//semaphores
//превращение указателя в смещение относительно начала разделяемой памяти
int get_offset(void* shm, void* ptr)
{
    return ((char*)ptr - (char*)shm);
}

//наоборот, превращение смещения в указатель
void* get_ptr(void* shm, int offset)
{
    if (offset == 0)
    {
        return NULL;
    }
    return (char*)shm + offset;
}

void producer_routine(void* shm, sem_t* sem)
{
    ShmHeader* header = (ShmHeader*)shm;
    srand(time(NULL) ^ getpid());

    printf("Producer started\n", getpid());
    
    while(!header->producer_finished)
    {
        int num_elements = rand() % MAX_ELEMENTS + 1;
        sem_wait(sem); // Wait for the semaphore before producing data
        int new_block_offset = header->free_block_offset;
        if (new_block_offset + sizeof(ShmDataBlock) > SHM_SIZE)
        {
            header->producer_finished = 1;
            sem_post(sem);
            printf("Producer finished, memory exhausted\n");
            break;
        }
        //something, idk

    }
}

int main(int argc, char **argv)
{
    printf("Hello, World!\n");
    return 0;
}
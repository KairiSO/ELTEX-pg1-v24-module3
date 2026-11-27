#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/mman.h>
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

// semaphores
// превращение указателя в смещение относительно начала разделяемой памяти
int get_offset(void *shm, void *ptr)
{
    return ((char *)ptr - (char *)shm);
}

// наоборот, превращение смещения в указатель
void *get_ptr(void *shm, int offset)
{
    if (offset == 0)
    {
        return NULL;
    }
    return (char *)shm + offset;
}

/*
Shared memory is divided into a header and a sequence of data blocks.
The header stores the offsets of the first and last blocks, the offset of
the next free block, and a flag indicating whether the producer has finished.
Each block contains a size field followed by its data. The producer appends
new blocks at the free-block offset and updates the last-block offset. The
consumer follows the first-block offset through the block chain to read data.
*/

void producer_routine(void *shm, sem_t *sem)
{
    ShmHeader *header = (ShmHeader *)shm;
    srand(time(NULL) ^ getpid());

    printf("[Producer PID: %d] started\n", getpid());

    while (!header->producer_finished)
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
        //we need to get pointer to the new block
        ShmDataBlock *new_block = (ShmDataBlock *)get_ptr(shm, new_block_offset);
        new_block->count = num_elements;
        new_block->next_block_offset = 0;
        for (int i = 0; i < num_elements; i++)
        {
            //we need to fill the data array with random values
            new_block->data[i] = rand() % 1000;
        }
        //now we need to insert the new block into the linked list
        if (header->first_block_offset == 0)
        {
            header->first_block_offset = new_block_offset;
        }
        else
        {
            ShmDataBlock *last_block = (ShmDataBlock *)get_ptr(shm, header->last_block_offset);
            last_block->next_block_offset = new_block_offset;
        }
        header->last_block_offset = new_block_offset;
        header->free_block_offset = new_block_offset + sizeof(ShmDataBlock);

        sem_post(sem); // Release the semaphore after finishing production
        printf("Producer finished writing in block of %d elements\n", num_elements);
        sleep(2); // Simulate some work after producing data
    }

    printf("[Producer PID: %d] finished\n", getpid());
}

void consumer_routine(void *shm, sem_t *sem)
{
    ShmHeader *header = (ShmHeader *)shm;
    printf("[Consumer PID: %d] started\n", getpid());

    while (!header->producer_finished)
    /*
    This loop continues until the producer has finished.
    Each iteration waits for the semaphore, processes the first non-empty block,
    and checks if all blocks have been processed.
    */
    {
        sem_wait(sem); // Wait for the semaphore before consuming data

        int processed_any = 0;
        int current_block_offset = header->first_block_offset;

        while(current_block_offset != 0)
        {
            ShmDataBlock *block = (ShmDataBlock *)get_ptr(shm, current_block_offset);

            if(block->count > 0)
            {
                //we need to find min and max values in the block
                int min = block->data[0];
                int max = block->data[0];
                for (int i = 1; i < block->count; i++)
                {
                    if (block->data[i] < min)
                    {
                        min = block->data[i];
                    }
                    if (block->data[i] > max)
                    {
                        max = block->data[i];
                    }
                }
                printf("[Consumer PID: %d] Block offset %d: Min = %d, Max = %d\n", getpid(), current_block_offset, min, max);
                block->count = 0;
                processed_any = 1;
                break;
            }
            current_block_offset = block->next_block_offset;
        }

        if(processed_any && header->producer_finished)
        {
            int all_done = 1;
            int check_offset = header->first_block_offset;
            while(check_offset != 0)
            {
                ShmDataBlock *block = (ShmDataBlock *)get_ptr(shm, check_offset);
                if(block->count > 0)
                {
                    all_done = 0;
                    break;
                }
                check_offset = block->next_block_offset;
            }
            if(all_done)
            {
                printf("All data has been processed\n");
                sem_post(sem);
                break;
            }
        }

        sem_post(sem); // Release the semaphore after finishing consumption

        if(!processed_any)
        {
            printf("[Consumer PID: %d] No data available\n", getpid());
            sleep(1); // Wait for data to be available
        }
        else
        {
            printf("[Consumer PID: %d] Data processed\n", getpid());
            sleep(1); // Simulate some work after consuming data
        }
    }

    printf("[Consumer PID: %d] finished\n", getpid());
}
/*
setup_shared_memory creates a POSIX shared memory object, sets its size,
and maps it into the process address space using mmap. The file descriptor
is returned through fd_out, and the mapped address is returned as the
function's result.
*/
void* setup_shared_memory(int* fd_out)
{
    int fd = shm_open("/my_shm", O_CREAT | O_RDWR, 0666);
    if(fd == -1)
    {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }

    if(ftruncate(fd, SHM_SIZE) == -1)
    {
        perror("ftruncate failed");
        exit(EXIT_FAILURE);
    }
    void *shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shm == MAP_FAILED)
    {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    *fd_out = fd;
    return shm;
}

//need to open POSIX semaphore
sem_t* setup_semaphore(int is_first)
{
    sem_t *sem;
    if(is_first)
    {
        sem = sem_open("/my_sem", O_CREAT, 0666, 1);
    }
    else
    {
        sem = sem_open("/my_sem", 0);
    }
    if(sem == SEM_FAILED)
    {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }
    return sem;
}

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Usage: %s <producer|consumer>\n", argv[0]);
        return 1;
    }

    int is_producer = (strcmp(argv[1], "producer") == 0);
    int shm_fd;
    void *shm = setup_shared_memory(&shm_fd);
    if(is_producer)
    {
        ShmHeader *header = (ShmHeader *)shm;
        if(header->free_block_offset == 0)
        {
            header->first_block_offset = 0;
            header->last_block_offset = 0;
            header->producer_finished = 0;
            header->free_block_offset = sizeof(ShmHeader);
        }
    }

    sem_t *sem = setup_semaphore(is_producer);

    if(is_producer)
    {
        producer_routine(shm, sem);
    }
    else
    {
        consumer_routine(shm, sem);
    }

    // Cleanup shared memory and semaphore
    munmap(shm, SHM_SIZE);
    close(shm_fd);
    sem_close(sem);
    if(is_producer)
    {
        sem_unlink("/my_sem");
        shm_unlink("/my_shm");
    }
    return 0;
}

/*
README.md

This program demonstrates inter-process communication using shared memory and POSIX semaphores.

Usage:
    ./program producer
    ./program consumer

Functionality:
    - Producer: Writes data to shared memory.
    - Consumer: Reads data from shared memory.
    - Shared memory is used to exchange data between producer and consumer.
    - POSIX semaphore is used to synchronize access to shared memory.

Notes:
    - The producer must be started first to initialize the shared memory and semaphore.
    - The consumer can be started multiple times to read data from shared memory.
    - Cleanup of shared memory and semaphore is handled by the producer.
*/
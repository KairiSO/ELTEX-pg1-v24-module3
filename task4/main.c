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
        // something, idk

        sem_post(sem); // Release the semaphore after finishing production
        printf("Producer finished writing in block of %d elements\n", num_elements);
        sleep(1); // Simulate some work after producing data
    }

    printf("[Producer PID: %d] finished\n", getpid());
}

void consumer_routine(void *shm, sem_t *sem)
{
    ShmHeader *header = (ShmHeader *)shm;
    printf("[Consumer PID: %d] started\n", getpid());

    while (!header->producer_finished)
    {
        sem_wait(sem); // Wait for the semaphore before consuming data

        int processed_any = 0;
        int current_block_offset = header->first_block_offset;

        while(current_block_offset != 0)
        {
            ShmDataBlock *block = (ShmDataBlock *)get_ptr(shm, current_block_offset);

            if(block->count > 0)
            {
                //idk, maybe process the data in the block
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
            printf("No data available\n");
            sleep(1); // Wait for data to be available
        }
        else
        {
            printf("Data processed\n");
            sleep(1); // Simulate some work after consuming data
        }
    }

    printf("[Consumer PID: %d] finished\n", getpid());
}

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
/* ipc.h - Протокол и каналы связи между процессами */
#ifndef IPC_H
#define IPC_H

#include <stddef.h>
#include <stdint.h>

#define MAX_FILENAME_LEN 256
#define BLOCK_SIZE       1024

/* Типы сообщений */
typedef enum {
    MSG_READY      = 1,  /* Дочерний готов принимать */
    MSG_FILE_INFO  = 2,  /* Имя файла + размер */
    MSG_DATA_BLOCK = 3,  /* Блок данных */
    MSG_FILE_DONE  = 4,  /* Файл передан целиком */
    MSG_ALL_DONE   = 5,  /* Все файлы переданы */
    MSG_ERROR      = 6
} MessageType;

/* Заголовок любого сообщения */
typedef struct {
    MessageType type;
    size_t      payload_len;
} MsgHeader;

/* Полезная нагрузка для MSG_FILE_INFO */
typedef struct {
    char   filename[MAX_FILENAME_LEN];
    size_t file_size;
} FileInfoPayload;

/* Пара каналов для двунаправленной связи */
typedef struct {
    int p2c_rfd;  /* Parent -> Child read  (читает дочерний) */
    int p2c_wfd;  /* Parent -> Child write (пишет родитель)  */
    int c2p_rfd;  /* Child  -> Parent read  (читает родитель) */
    int c2p_wfd;  /* Child  -> Parent write (пишет дочерний)  */
} ChannelHandles;

/* Создать каналы. base_name == NULL -> неименованные pipe. */
int  init_channels(const char* base_name, ChannelHandles* h);
void close_unused_ends(ChannelHandles* h, int is_parent);
void close_all_channels(ChannelHandles* h);

int           send_message(int fd, MessageType type, const void* payload, size_t len);
MessageType   receive_message(int fd, MsgHeader* hdr_out, void* buf, size_t buf_size);

#endif
#ifndef TCP_CHAT_H
#define TCP_CHAT_H

//最大连接数为64
#define MAX_CONNECT_NUM  64

#define MAX_LINE_VAL  1024

typedef struct {
    int fd;
    int flag;
    int state;
    int is_lock;
    char name[20];
    char pass[20];
} client_info_t;

#endif

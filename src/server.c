#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "chat.h"

client_info_t only_client[MAX_CONNECT_NUM];
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

// 初始化tcp
int tcp_init()
{
    struct sockaddr_in servaddr;
    int listenfd;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        fprintf(stderr, "%s\n", "socket init fail");
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8080);
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "%s\n", "bind init fail");
        close(listenfd);
        return -1;
    }

    if (listen(listenfd, 10) < 0) {
        fprintf(stderr, "%s\n", "listen init fail");
        close(listenfd);
        return -1;
    }

    return listenfd;
}

// 解析命令
int parse(char *str, char *res, char *dest)
{
    int state = 0;
    char *ptr;

    // 判断是否是合法命令
    if (str[0] != '#') {
        state = -1;
    } else {
        str++;
        ptr = strchr(str, ' ');
        if (ptr != NULL) {
            memcpy(res, str, ptr - str);
            res[ptr - str] = '\0'; // 手动添加字符串终止符
            strcpy(dest, ptr + 1); // 复制空格后的字符串
            dest[strlen(ptr + 1)] = '\0'; // 在dest最后添加'\0'
        } else {
            strcpy(res, str); // 如果没有空格，复制整个字符串
            dest = NULL;
        }
    }
    return state;
}

int find_current_user(char *name)
{
    for (int i = 0; i < MAX_CONNECT_NUM; i++) {
        if (only_client[i].flag == -1) {
            continue;
        }
        if (strcmp(name, only_client[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

int get_user_state(char *name)
{
    int i = 0;

    for (i = 0; i < MAX_CONNECT_NUM; i++) {
        if (only_client[i].state == -1) {
            continue;
        }

        int ret = memcmp(only_client[i].name, name, strlen(name));
        if (ret == 0) {
            return i;
        }
    }

    return -1;
}

int set_user_name(int sfd, char *name, char *err)
{
    int n, i;
    char *ptr;

    if (strlen(name) == 0) {
        n = sprintf(err, "%s", "命令错误，示例：#setname <user_name>\n\n");
        err[n] = '\0';
        return -1;
    }

    ptr = strchr(name, ' ');
    if (ptr != NULL) {
        n = sprintf(err, "%s", "The name cannot contain Spaces!\n\n");
        err[n] = '\0';
        return -1;
    }

    if (strlen(name) > 20) {
        n = sprintf(err, "%s", "Username is too long!\n\n");
        err[n] = '\0';
        return -1;
    }

    pthread_mutex_lock(&client_lock);
    if (find_current_user(name) != -1) {
        n = sprintf(err, "%s", "Username is exist!\n\n");
        err[n] = '\0';
        pthread_mutex_unlock(&client_lock);
        return -1;
    }

    for (i = 0; i < MAX_CONNECT_NUM; i++) {
        if (only_client[i].flag == -1) {
            only_client[i].fd = sfd;

            memcpy(only_client[i].name, name, strlen(name));
            only_client[i].name[strlen(name)] = '\0'; // 添加字符串终止符

            only_client[i].flag = 1;
            only_client[i].state = -1;
            printf("set only_client[%d].name = %s\n", i, name);
            pthread_mutex_unlock(&client_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&client_lock);
    return -1;
}

int login(int sfd, int index, char *name, char *err)
{
    int n, i;
    if (only_client[index].state != -1 && index > 0) {
        n = sprintf(err, "%s", "你已经登录过了，不要重复登录（别偷摸登别人号）\n\n");
        err[n] = '\0';
        return -1;
    }

    if (strlen(name) == 0) {
        n = sprintf(err, "%s", "命令错误，示例：#login <user_name>\n\n");
        err[n] = '\0';
        return -1;
    }

    pthread_mutex_lock(&client_lock);
    for (i = 0; i < MAX_CONNECT_NUM; i++) {
        if (only_client[i].flag == -1) {
            continue;
        }
        if ((memcmp(name, only_client[i].name, strlen(name)) == 0) && (only_client[i].state == -1)) {
            only_client[i].state = 1;
            only_client[i].fd = sfd;
            pthread_mutex_unlock(&client_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&client_lock);

    n = sprintf(err, "%s", "Username inexistence!\n\n");
    err[n] = '\0';
    return -1;
}

//每个人退出自己登录
void logout(int sfd, int *index, char *err)
{
    int n, i;

    if (*index < 0) {
        n = sprintf(err, "%s", "You are not logged in yet, no need to log out!\n\n");
        err[n] = '\0';
        return;
    }

    pthread_mutex_lock(&client_lock);
    only_client[*index].fd = -1;
    only_client[*index].state = -1;
    pthread_mutex_unlock(&client_lock);

    *index = 0;
    return;
}

int private_user(int from_index, char *data, char *err)
{
    char name[20], from_name[20];
    char *ptr;
    int index, n;

    char buf[MAX_LINE_VAL];

    if (strlen(data) == 0) {
        n = sprintf(err, "%s", "命令错误，示例：#private <user_name> <string>\n\n");
        err[n] = '\0';
        return -1;
    }

    ptr = strchr(data, ' ');
    if (ptr != NULL) {
        // 复制私聊名字
        strncpy(name, data, ptr - data);
        name[ptr - data] = '\0';  // 添加字符串终止符

        // 移动指针到消息部分
        data = ptr + 1;

        printf("name: %s, strlen(name) = %ld, data: %s\n", name, strlen(name), data);

        pthread_mutex_lock(&client_lock);
        // 查询该用户是否在线
        index = get_user_state(name);
        if (index == -1) {
            n = sprintf(err, "The user %s is not online or inexistence!\n\n", name);
            err[n] = '\0';
            pthread_mutex_unlock(&client_lock);
            return -1;
        }

        memcpy(from_name, only_client[from_index].name, strlen(only_client[from_index].name));
        from_name[strcspn(from_name, "\n")] = ':';
        n = snprintf(buf, MAX_LINE_VAL, "from %s %s\n\n", from_name, data);
        buf[n] = '\0';

        // 发送消息给指定用户
        int send_ret = send(only_client[index].fd, buf, strlen(buf), 0);
        if (send_ret == -1) {
            perror("send failed");
            pthread_mutex_unlock(&client_lock);
            return -1;
        }
        pthread_mutex_unlock(&client_lock);
    } else {
        n = sprintf(err, "Incorrect usage\n\n");
        err[n] = '\0';
        return -1;
    }

    return 0;
}

void *handler(void *arg)
{
    char buf[MAX_LINE_VAL];
    int sfd = *(int *)arg;
    free(arg);
    int index = -1, ret, ans;

    while (1) {
        char cmd[20];
        char data[MAX_LINE_VAL];
        char err[128];
        char ok[40];
        memset(err, 0, 128);
        memset(cmd, 0, 20);
        memset(ok, 0, 40);
        memset(data, 0, MAX_LINE_VAL);

        memset(buf, 0, MAX_LINE_VAL);
        // 从客户端读取数据
        ret = recv(sfd, buf, MAX_LINE_VAL, 0);
        if (ret <= 0) {
            // 断开连接
            if (index >= 0) {
                pthread_mutex_lock(&client_lock);
                only_client[index].fd = -1;
                only_client[index].state = -1;
                pthread_mutex_unlock(&client_lock);
            }
            close(sfd);
            break;
        }

        // 解析数据
        ans = parse(buf, cmd, data);
        if (ans != 0) {
            char *err_msg = "input cmd error\n\n";
            send(sfd, err_msg, strlen(err_msg), 0);
            continue;
        }

        cmd[strcspn(cmd, "\n")] = '\0';
        if (!strcmp(cmd, "setname")) {
            index = set_user_name(sfd, data, err);
            // 设置用户名
            if (-1 == index) {
                write(sfd, err, strlen(err));
            } else {
                snprintf(ok, 40, "%s\n\n", "setname success!");
                write(sfd, ok, sizeof(ok));
            }
        } else if (!strcmp(cmd, "login")) {
            // 登录
            if (-1 == login(sfd, index, data, err)) {
                write(sfd, err, strlen(err));
            } else {
                snprintf(ok, 40, "%s\n\n", "login success!");
                write(sfd, ok, sizeof(ok));
            }
        } else if (!strcmp(cmd, "private")) {
            ret = private_user(index, data, err);
            if (-1 == ret) {
                write(sfd, err, strlen(err));
            }
        } else if (!memcmp(cmd, "logout", strlen(cmd))) {
            logout(sfd, &index, err);
            if (-1 == index) {
                write(sfd, err, strlen(err));
            } else {
                snprintf(ok, 40, "%s\n\n", "logout success!");
                write(sfd, ok, sizeof(ok));
                index = -1;
            }
        } else {
            char *str = "undefine cmd\n\n";
            write(sfd, str, strlen(str));
        }
    }

    return NULL;
}

int main()
{
    int i;
    socklen_t clilen;
    struct sockaddr_in client_addr;
    int sockfd = tcp_init();
    if (-1 == sockfd) {
        return -1;
    }

    // 清空用户信息
    for (i = 0; i < MAX_CONNECT_NUM; i++) {
        only_client[i].flag = -1;
        only_client[i].fd = -1;
        only_client[i].state = -1;
    }

    while (1) {
        clilen = sizeof(client_addr);
        int cfd = accept(sockfd, (struct sockaddr *)&client_addr, &clilen);
        if (cfd < 0) {
            fprintf(stderr, "%s\n", "accept fail");
            continue;
        }
        char buf[MAX_LINE_VAL];
        printf("Connect form %s, port %d\n",
               inet_ntop(AF_INET, &client_addr.sin_addr, buf, sizeof(buf)),
               ntohs(client_addr.sin_port));

        // 创建线程处理每个客户端
        pthread_t tid;

        int *fd = (int *)malloc(sizeof(int));
        if (fd == NULL) {
            perror("malloc failed");
            close(cfd);
            continue;
        }
        *fd = cfd;
        pthread_create(&tid, NULL, handler, (void *)fd);

        pthread_detach(tid); // 线程分离
    }

    close(sockfd);
    return 0;
}
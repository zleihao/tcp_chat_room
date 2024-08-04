#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "chat.h"

#define SERVER_PORT 8080

client_info_t only_client[MAX_CONNECT_NUM];
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 初始化socket
 * @param null
 * @retval 监听的 socket 描述符
 */
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
    servaddr.sin_port = htons(SERVER_PORT);
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

/**
 * @brief 解析输入的指令
 * @param str: 客户端发送的数据。
 * @param res: 解析到的指令。
 * @param dest: 解析到的数据部分。
 * @retval 客户发送的数据是否符合规则。
 */
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

/**
 * @brief 查找指定用户的索引
 * @param name: 查找的用户名。
 * @retval 若是存在该用户则返回该用户的索引，若不存在则返回-1.
 */
int find_current_user(char *name)
{
    for (int i = 0; i < MAX_CONNECT_NUM; i++) {
        if (only_client[i].flag == -1) {
            continue;
        }
        if (strncmp(name, only_client[i].name, strlen(only_client[i].name)) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 查询指定用户当前是否在线。
 * @param name: 查询的用户名。
 * @retval 若该用户在线则返回该用户的索引，若不在线则返回-1.
 */
int get_user_state(char *name)
{
    int i = 0;

    for (i = 0; i < MAX_CONNECT_NUM; i++) {
        //过滤不在线用户
        if (only_client[i].state == -1) {
            continue;
        }

        int ret = memcmp(only_client[i].name, name, strlen(only_client[i].name));
        if (ret == 0) {
            return i;
        }
    }

    return -1;
}

int set_user_name(int sfd, char *data, char *err)
{
    int n, i;
    char *ptr;
    char password[4] = "123";
    char name[20];

    if (strlen(data) == 0) {
        n = sprintf(err, "%s", "命令错误，示例：#setname <user_name>\n\n");
        err[n] = '\0';
        return -1;
    }

    ptr = strchr(data, ' ');
    if (ptr != NULL) {
        n = sprintf(err, "%s", "设置的昵称中不能含有空格!\n\n");
        err[n] = '\0';
        return -1;
    }

    if (strlen(data) > 20) {
        n = sprintf(err, "%s", "设置的昵称过长!\n\n");
        err[n] = '\0';
        return -1;
    }

    ptr = strtok(data, " \n");
    strcpy(name, ptr);

    pthread_mutex_lock(&client_lock);
    if (find_current_user(name) != -1) {
        n = sprintf(err, "%s", "该昵称已存在!\n\n");
        err[n] = '\0';
        pthread_mutex_unlock(&client_lock);
        return -1;
    }

    for (i = 0; i < MAX_CONNECT_NUM; i++) {
        if (only_client[i].flag == -1) {
            only_client[i].fd = sfd;

            strcpy(only_client[i].name, name);
            // 设置默认密码
            strncpy(only_client[i].pass, "123", sizeof(only_client[i].pass));
            only_client[i].flag = 1;
            only_client[i].state = -1;
            printf("set only_client[%d].name = %s, only_client[%d].pss = %s, flag = %d\n", i, name, i, only_client[i].pass, only_client[i].flag);
            pthread_mutex_unlock(&client_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&client_lock);
    return -1;
}

void print_hex(const char *str, int len)
{
    for (int i = 0; i < len; i++) {
        printf("%02x ", (unsigned char)str[i]);
    }
    printf("\n");
}

int login(int sfd, int *index, char *data, char *err)
{
    int n, i, flage = 0;
    char *ptr;
    char name[20], pass[20];

    if (only_client[*index].state != -1 && *index >= 0) {
        n = sprintf(err, "%s", "你已经登录过了，不要重复登录（别偷摸登别人号）\n\n");
        err[n] = '\0';
        return -1;
    }

    if (strlen(data) == 0) {
        n = sprintf(err, "%s", "命令错误，示例：#login <user_name> <password>\n\n");
        err[n] = '\0';
        return -1;
    }

    if (ptr != NULL) {
        ptr = strtok(data, " \n");
        // 复制私聊名字
        strcpy(name, ptr);

        //得到密码部分
        ptr = strtok(NULL, " \n");
        strcpy(pass, ptr);
        printf("name: %s, pass: %s\n", name, pass);
        pthread_mutex_lock(&client_lock);
        for (i = 0; i < MAX_CONNECT_NUM; i++) {
            if (only_client[i].flag == -1) {
                continue;
            }
            print_hex(pass, strlen(pass));
            print_hex(only_client[i].pass, strlen(only_client[i].pass));

            if (((!strcmp(name, only_client[i].name))) &&
                (!strcmp(pass, only_client[i].pass))) {
                if (-1 == only_client[i].state) {
                    only_client[i].state = 1;
                    only_client[i].fd = sfd;
                    pthread_mutex_unlock(&client_lock);
                    *index = i;
                    return i;
                } else {
                    flage = 1;
                }
            }
        }
        pthread_mutex_unlock(&client_lock);
    } else {
        n = sprintf(err, "Incorrect usage\n\n");
        err[n] = '\0';
        *index = -1;
        return -1;
    }

    if (flage) {
        n = sprintf(err, "%s", "该用户已经登录!\n\n");
        err[n] = '\0';
    } else {
        n = sprintf(err, "%s", "该昵称不存在或者密码错误，请重试！\n\n");
        err[n] = '\0';
    }

    return -1;
}

//每个人退出自己登录
int logout(int sfd, int *index, char *err)
{
    int n, i;

    printf("logout: index = %d, state = %d\n", *index, only_client[*index].state);
    if (*index >= 0 && (only_client[*index].state == 1)) {
        pthread_mutex_lock(&client_lock);
        only_client[*index].fd = -1;
        only_client[*index].state = -1;
        pthread_mutex_unlock(&client_lock);

        *index = -1;
        return 0;
    }

    n = sprintf(err, "%s", "您还没有登录，不需要注销!\n\n");
    err[n] = '\0';
    return -1;
}

//修改密码
int changepass(int *index, char *data, char *err)
{
    int n, i;
    char pass_old[20], pass_new[20];
    char *ptr;

    printf("%s\n", data);

    //得到旧密码
    ptr = strtok(data, " \n");
    if (NULL == ptr) {
        n = sprintf(err, "用法有误，请使用 #changepass <old pass> <new pass>\n\n");
        err[n] = '\0';
        return -1;
    }
    snprintf(pass_old, 20, "%s", ptr);

    //得到新密码
    ptr = strtok(NULL, " \n");
    if (NULL == ptr) {
        n = sprintf(err, "用法有误，请使用 #changepass <old pass> <new pass>\n\n");
        err[n] = '\0';
        return -1;
    }
    snprintf(pass_new, 20, "%s", ptr);

    ptr = strtok(NULL, " \n");
    if (ptr != NULL) {
        n = sprintf(err, "用法有误，请使用 #changepass <old pass> <new pass>\n\n");
        err[n] = '\0';
        return -1;
    }

    printf("修改密码: old pass %s, new pass %s\n", pass_old, pass_new);

    pthread_mutex_lock(&client_lock);
    if (*index < 0) {
        pthread_mutex_unlock(&client_lock);
        n = sprintf(err, "请登录后再进行修改密码\n\n");
        err[n] = '\0';
        return -1;
    } else {
        if (-1 == only_client[*index].state) {
            pthread_mutex_unlock(&client_lock);
            n = sprintf(err, "请登录后再进行修改密码\n\n");
            err[n] = '\0';
            return -1;
        }
        //直接修改当前用户的密码
        if (!strcmp(pass_old, only_client[*index].pass)) {
            //旧密码正确，修改密码
            strcpy(only_client[*index].pass, pass_new);
            //密码已经更改请重新登录
            only_client[*index].state = -1;
            *index = -1;
        } else {
            pthread_mutex_unlock(&client_lock);
            n = sprintf(err, "输入的旧密码不正确\n\n");
            err[n] = '\0';
            return -1;
        }
    }
    pthread_mutex_unlock(&client_lock);

    return 0;
}

int private_user(int from_index, char *data, char *err)
{
    char name[20], from_name[20];
    char *ptr;
    int index, n;

    char buf[MAX_LINE_VAL];

    if (strlen(data) == 0) {
        n = sprintf(err, "%s", "用法错误，示例：#private <user_name> <string>\n\n");
        err[n] = '\0';
        return -1;
    }

    if (from_index < 0) {
        n = sprintf(err, "%s", "请登录后再进行聊天，Thanks\n\n");
        err[n] = '\0';
        return -1;
    }

    if (only_client[from_index].state == -1) {
        n = sprintf(err, "%s", "请登录后再进行聊天，Thanks\n\n");
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
            n = sprintf(err, "该用户 %s 当前不在线或者不存在该用户!\n\n", name);
            err[n] = '\0';
            pthread_mutex_unlock(&client_lock);
            return -1;
        }

        memcpy(from_name, only_client[from_index].name, strlen(only_client[from_index].name));
        from_name[strcspn(from_name, "\n")] = ':';
        n = snprintf(buf, MAX_LINE_VAL, "\033[33mfrom %s\033[0m \033[32m%s\033[0m\n\n", from_name, data);
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
        n = sprintf(err, "%s", "用法错误，示例：#private <user_name> <string>\n\n");
        err[n] = '\0';
        return -1;
    }

    return 0;
}

int is_all_spaces(char *str)
{
    if (str == NULL) {
        return 0; // or handle error
    }

    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return 0; // 不是空格字符
        }
        str++;
    }

    return 1; // 全是空格字符
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
        char ok[1024];
        char *ptr;
        memset(err, 0, 128);
        memset(cmd, 0, 20);
        memset(ok, 0, 1024);
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
            printf("%s 退出\n\n", only_client[index].name);
            break;
        }

        //判断用户输入
        if (is_all_spaces(buf)) {
            continue;
        }
        if (buf[0] == '\n') {
            continue;
        }

        // 解析数据
        ans = parse(buf, cmd, data);
        if (ans != 0) {
            char *err_msg = "未知的命令\n\n";
            send(sfd, err_msg, strlen(err_msg), 0);
            continue;
        }

        cmd[strcspn(cmd, "\n")] = '\0';
        if (!strcmp(cmd, "help")) {
            snprintf(ok, MAX_LINE_VAL, "目前聊天室支持以下指令：\n\
                                        \033[32m\r注册：\t    #setname <user name>\n\
                                        \r修改密码:   #changepass <old pass> <new pass>\n\
                                        \r登录：\t    #login <user name> <pass>\n\
                                        \r注销：\t    #logout\n\
                                        \r私聊：\t    #private <object name> <string>\033[0m\n\n");

            write(sfd, ok, strlen(ok));
        } else if (!strcmp(cmd, "setname")) {
            index = set_user_name(sfd, data, err);
            // 设置用户名
            if (-1 == index) {
                write(sfd, err, strlen(err));
            } else {
                snprintf(ok, 1024, "%s\n\n", "用户名设置成功!\n默认密码为： \"123\"\n修改密码指令：#changepass <old pass> <new pass>");
                write(sfd, ok, sizeof(ok));
            }
        } else if (!strcmp(cmd, "login")) {
            // 登录
            if (-1 == login(sfd, &index, data, err)) {
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
            if (-1 == logout(sfd, &index, err)) {
                write(sfd, err, strlen(err));
            } else {
                snprintf(ok, 40, "%s\n\n", "logout success!");
                write(sfd, ok, sizeof(ok));
                index = -1;
            }
        } else if (!memcmp(cmd, "changepass", strlen(cmd))) {
            if (-1 == changepass(&index, data, err)) {
                write(sfd, err, strlen(err));
            } else {
                snprintf(ok, 1024, "%s\n\n", "密码更改成功...\n请使用新的密码重新登录...\n\n");
                write(sfd, ok, sizeof(ok));
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
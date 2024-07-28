#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define MAXLINE 2048

#define max(x, y) ((x) > (y) ? x : (y))

void str_in(int fd)
{
   char tx_buf[MAXLINE], rx_buf[MAXLINE];
   int maxfdpl, stdineof;
   fd_set rset;
   int n;

   stdineof = 0;
   FD_ZERO(&rset);

   while (1) {
      if (0 == stdineof) {
     		FD_SET(fileno(stdin), &rset);
        }
        FD_SET(fd, &rset);
      maxfdpl = max(fileno(stdin), fd) + 1;
       select(maxfdpl, &rset, NULL, NULL, NULL);

       if (FD_ISSET(fd, &rset)) {
           if ((n = read(fd, rx_buf, MAXLINE)) == 0) {
               if (1 == stdineof) {
                   return;
               } else {
                   fprintf(stderr, "str_cli: server terminated prematurely");
                   break;
               }
           }
           write(fileno(stdout), rx_buf, n);
       }

       if (FD_ISSET(fileno(stdin), &rset)) {
           if ((n = read(fileno(stdin), rx_buf, MAXLINE)) == 0) {
               stdineof = 1;
               shutdown(fd, SHUT_WR);
               FD_CLR(fileno(stdin), &rset);
               continue;
           }
           write(fd, rx_buf, n);
       }
   }

}

int main(int argc, char *argv[])
{
    int sockfd, cfd;
    struct sockaddr_in client_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("socket error\n");
        return 0;
    }

    bzero(&client_addr, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, argv[1], &client_addr.sin_addr);

    cfd = connect(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr));
    if (cfd < 0) {
        printf("connect error\n");
        return 0;
    }

    str_in(sockfd);
    exit(0);
}
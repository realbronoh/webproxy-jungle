#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>


#define MAXLEN 100

int main(int argc, char *argv[]){
    int ip_addr;
    struct addrinfo hints, *listp, *p;  // 결과를 저장할 변수
    char ip_addr_dec[30], buf[MAXLEN], canon_name[30];

    int flags;

    memset(&hints, 0, sizeof(hints));   // hints 구조체의 모든 값을 0으로 초기화
    hints.ai_family = AF_UNSPEC;        // IPv4와 IPv6 모두 받음
    hints.ai_socktype = SOCK_STREAM;     // TCP stream socket
    hints.ai_flags |= AI_CANONNAME;
    getaddrinfo(argv[1], "http", &hints, &listp);

    // walk the list and display each IP address
    for (p = listp; p; p = p->ai_next){
        ip_addr = ((struct sockaddr_in *)(p->ai_addr))->sin_addr.s_addr;
        inet_ntop(AF_INET, &ip_addr, ip_addr_dec, 30);

        printf("%s\n", ip_addr_dec);
    }

    // // walk the list and display each IP address
    // for (p = listp; p; p = p->ai_next){
    //     flags = NI_NUMERICHOST;
    //     getnameinfo(p->ai_addr, p->ai_addrlen, buf, MAXLEN, NULL, 0, flags);
    //     printf("%s\n", buf);
    // }
    freeaddrinfo(listp);
    return 0;
}



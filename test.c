#include "csapp.h"
// #include <stdio.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netdb.h>
// #include <string.h>


int main(int argc, char **argv){
    struct addrinfo hints, *listp, *p;
    int listenfd;
    char *port = "80";

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* Accept connections */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV;            /* ... using port number */
    Getaddrinfo(NULL, port, &hints, &listp);
    
    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next) {
        printf("%x seo\n", ((struct sockaddr_in *)(p->ai_addr))->sin_addr.s_addr);
    }

    return 0; 
}
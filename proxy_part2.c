#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";
void *thread(void *vargp);

void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio);
int connect_endServer(char *hostname, int port, char *http_header);

int main(int argc,char **argv) {

    int listenfd, connfd;
    socklen_t  clientlen;
    char hostname[MAXLINE], port[MAXLINE];
    pthread_t tid;
    struct sockaddr_storage clientaddr;     /* generic sockaddr struct which is 28 Bytes. The same use as sockaddr */

    if (argc != 2) {
        fprintf(stderr, "usage :%s <port> \n", argv[0]);
        exit(1);
    }

    // open a listening socket
    listenfd = Open_listenfd(argv[1]);
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        /* print accepted message */
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s %s).\n", hostname, port);

        // 각 스레드마다 doit()함수 실생 --> 트랜잭션 처리
        Pthread_create(&tid, NULL, thread, (void *)connfd);
    }
    return 0;
}

/* thread function */
// 각 스레드마다 실질적인 doit() 함수 실행 --> 트랜잭션 처리
void *thread(void *vargp){
    int connfd = (int)vargp;
    // detach 모드: 종료시 커널이 알아서 처리해줌?
    Pthread_detach(pthread_self());
    doit(connfd);
    Close(connfd);
}

/* handle the client HTTP transaction */
// part1과 동일
void doit(int connfd)
{
    int end_serverfd; /* the end server file descriptor */

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char endserver_http_header[MAXLINE];

    /* store the request line arguments */
    char hostname[MAXLINE], path[MAXLINE];
    int port;

    rio_t rio, server_rio; /* rio is client's rio, server_rio is endserver's rio*/

    /* connection to client */
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version); /* read the client request line */

    /* only accept GET method */
    if (strcasecmp(method, "GET")) {
        printf("Proxy does not implement the method");
        return;
    }
    /* parse the uri to get hostname, file path, port */
    parse_uri(uri, hostname, path, &port);

    /* build the http header which will send to the end server */
    build_http_header(endserver_http_header, hostname, path, port, &rio);

    /* connect to the end server */
    end_serverfd = connect_endServer(hostname, port, endserver_http_header);

    if (end_serverfd < 0) {
        printf("connection failed\n");
        return;
    }

    /* connection to end_server */
    Rio_readinitb(&server_rio, end_serverfd);
    /* write the http header to endserver */
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

    /* receive message from end server and send to the client */
    size_t n;
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        printf("proxy received %d bytes, then send\n", n);
        Rio_writen(connfd, buf, n);
    }
    Close(end_serverfd);
}

/* client request를 바탕으로 End server에 보낼 헤더 만드는 함수 */
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio) {

    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

    /* request line */
    sprintf(request_hdr, requestlint_hdr_format, path);

    /* get other request header for client rio and change it */
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {

        /* End of request header */
        if (strcmp(buf, endof_hdr) == 0)  break;

        /* Host */
        if (!strncasecmp(buf, host_key, strlen(host_key))){
            strcpy(host_hdr, buf);
            continue;
        }
        /* Other headers: Connection, Proxy-Connection, User-Agent */
        if (!strncasecmp(buf, connection_key, strlen(connection_key))
                &&!strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))
                &&!strncasecmp(buf, user_agent_key, strlen(user_agent_key))) {
            strcat(other_hdr, buf);
        }
    }

    /* fill in Host-tag if it is empty */
    if (strlen(host_hdr) == 0) {
        sprintf(host_hdr, host_hdr_format, hostname);
    }

    /* concatenate all header tags into http_header */
    sprintf(http_header,"%s%s%s%s%s%s%s",
            request_hdr,    // "GET file-path HTTP/1.0"
            host_hdr,       // "Host"
            conn_hdr,       // "Connection: close\r\n"
            prox_hdr,       // Proxy-Connection: close\r\n"
            user_agent_hdr, // "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n"
            other_hdr,      // "Connection; Proxy-Connection; User-Agent"
            endof_hdr);     // "\r\n"
    return;
}

/* Connect to the end server */
// inline함수: 코드를 그대로 복제 --> 실행속도가 빠르나, 코드가 길어져 실행파일의 크기가 커진다,
inline int connect_endServer(char *hostname, int port, char *http_header){
    char portStr[100];
    sprintf(portStr, "%d", port);
    return Open_clientfd(hostname, portStr);    // end_server에게 proxy는 client
}

/* parse the uri to get hostname, file path, port */
void parse_uri(char *uri, char *hostname, char *path, int *port) {
    // 일반적인 URI format: http://host:port/path?query_string

    *port = 80;
    char* pos = strstr(uri, "//");

    pos = pos != NULL ? pos+2 : uri;    // pos: uri에서 "http://" 바로 뒷부분

    char *pos2 = strstr(pos, ":");      // port번호 있는지 여부
    // port번호 있을때: "host:port/path?query_string"
    if (pos2 != NULL) {
        *pos2 = '\0';
        sscanf(pos, "%s", hostname);        // "host"
        sscanf(pos2+1, "%d%s", port, path); // "port/path?query_string"
    }
    // port번호 없을때: "host/path?query_string"
    else {
        pos2 = strstr(pos, "/");
        // path 있을때
        if (pos2 != NULL) {
            *pos2 = '\0';       // 먼저 host알아내기 위해 문자열 분리
            sscanf(pos,"%s", hostname);
            *pos2 = '/';        // '/'복구 후 path기록
            sscanf(pos2,"%s", path);
        }
        // path 없을때: "host"
        else {
            sscanf(pos,"%s", hostname);
        }
    }
}
/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void serve_head_method(int fd, char *filename, int filesize);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* Open a listening socket */
  listenfd = Open_listenfd(argv[1]);

  /* Infinite loop: repeatedly accept a connection request, 
   *                perform a transaction, and close connection */
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // accept connection request
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, // client info
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // transaction
    Close(connfd);  // close
  }
}

void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);            // initiation: rio와 fd 연결
  Rio_readlineb(&rio, buf, MAXLINE);  // read and parse the request line
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);  // buf의 내용을 각각 method, uri, version에 저장

  // Tiny only acceps the "GET" method & "HEAD" method
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")){   // strcasecmp: 대소문자 구분 없이 두 문자열 비교
    clienterror(fd, method, "501", "Not implemented",
                "Tiny does not implement this method");
    return;
  }

  /* Read and ignore request headers */
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);  // flags indicating whether request is for static or dynamic content
  if (stat(filename, &sbuf) < 0){  // get file information and save it to sbuf(struct stat)
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  /* Serve HEAD method */
  if (!strcasecmp(method, "HEAD")){
    serve_head_method(fd, filename, sbuf.st_size);
    return;
  }

  /* Serve static content */
  if (is_static){
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){  // regular file and permission to read
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the fuke");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  /* Serve dynamic content */
  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){ // executable file
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

void clienterror(int fd, char *cause, char*errnum, char *shortmsg, char *longmsg){

  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body,  "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body,  "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body,  "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body,  "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  // send header
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  // send body
  Rio_writen(fd, body, strlen(body));
}

/* Ignore header: TINY does not use header */
void read_requesthdrs(rio_t *rp){

  char buf[MAXLINE];

  // header ends with '\r\n'
  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {

  char *ptr;

  /* Static content */
  if (!strstr(uri, "cgi-bin")) {       // strstr(대상문자열, 검색할문자열) --> 문자열의 시작 포인터 or NULL 반환
    strcpy(cgiargs, "");               // clear CGI argument
    strcpy(filename, ".");             // convert URI into relative LINUX pathname
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/')   // default filename: when URI ends with '/'
      strcat(filename, "home.html");
    return 1;
  }
  /* Dynamic content */
  else {
    ptr = index(uri, '?');      // find index of CGI arguments
    if (ptr) {
      strcpy(cgiargs, ptr + 1); // extract CGI arguments
      *ptr = '\0';              // 문자열 분리(uri string & CGI args string)
    }
    else
      strcpy(cgiargs, "");      // no CGI argument
    strcpy(filename, ".");      // convert URI into LINUX pathname  --> './uri'로 바꾸는듯
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize) {

  int srcfd;    // source file descriptor
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  // make header and save at 'buf'
  get_filetype(filename, filetype);   // filename에서 filetype 추출해 저장
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  // write header to fd(send header to client)
  Rio_writen(fd, buf, strlen(buf));
  // print header in stdout(server)
  printf("Response headers:\n");
  printf("%s", buf);

  // 원래 코드(문제 11.9 때문에 주석처리함)
  // /* Send reponse body to client */
  // srcfd = Open(filename, O_RDONLY, 0);  // 파일 열기
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 가상메모리에 메모리 매핑
  // // 0(NULL): 적절한 위치에 메모리 매핑 --> srcp에 시작 주소 반환
  // // PROT_READ: READ_ONLY
  // // MAP_PRIVATE: 데이터 변경 내용 공유하지 않음(최초 쓰기시 사본 생성)
  // Close(srcfd);                     // 메모리 매핑 이후 file descriptor 필요 없음 --> close file
  // Rio_writen(fd, srcp, filesize);   // send file data to client
  // Munmap(srcp, filesize);           // 메모리 매핑 해제

  /* Problem 11.9: malloc함수 사용 */
  srcfd = Open(filename, O_RDONLY, 0);
  void *ptr = Malloc(filesize);
  int read_byte;
  if ((read_byte = Rio_readn(srcfd, (char *)ptr, filesize)) < 0){
    printf("file reading failed !\n");
  }
  Close(srcfd);
  Rio_writen(fd, (char *)ptr, filesize);
  Free(ptr);
}

/*
 * get_filetype: Derive file type from filename
 */
void get_filetype(char *filename, char* filetype) {

  if (strstr(filename, ".html"))      // strstr(대상문자열, 검색할문자열) --> 문자열의 시작 포인터 or NULL 반환
    strcpy(filetype, "text/html");    // strcpy(char *dest, const char *origin) --> origin문자열을 dest로 복사
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  /* Problem 11.7: add mpg filetype */
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) {

  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  /* Child */
  if (Fork() == 0) {
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);     // 새로운 환경변수 추가
    Dup2(fd, STDOUT_FILENO);                /* Redirect stdout to client */
    Execve(filename, emptylist, environ) ;  /* Run CGI prigram */
  }
  Wait(NULL);   /* Parent waits for and reaps child */
}

void serve_head_method(int fd, char *filename, int filesize) {

  char filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  // make header and save at 'buf'
  get_filetype(filename, filetype);   // filename에서 filetype 추출해 저장
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  // write header to fd(send header to client)
  Rio_writen(fd, buf, strlen(buf));
  // print header in stdout(server)
  printf("Response headers:\n");
  printf("%s", buf);
}
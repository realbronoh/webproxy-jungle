//tiny.c - a simple, iterative HTTP/1.0 Web server that uses the GET method to serve static and dynamic content

#include "csapp.h"

void echo(int connfd);
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

// main 요약) Open_listenfd 함수를 사용해 listenfd를 생성하고,
// accept함수를 통해 연결 요청한 clientfd와 연결
// accept 함수에서 clientaddr에 저장한 client의 주소를 getnameinfo 함수 인자로 넣어
// 서버의 ip 주소와 포트 번호를 얻고 이를 print
// 다음으로 client에서 받은 요청을 처리하는 doit 함수 진행
int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; //클라이언트의 주소 정보를 담고 있는 구조체

    // check command-line args
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]); //인자로 받은 port에 listenfd 생성
    while (1) {
        clientlen = sizeof(clientaddr);
        // listenfd에 연결 요청한 client의 주소를 sockaddr_storage에 저장
        // client의 주소, 크기를 받아 저장할 곳의 포인터를 인자로 받음
        // accept의 세번째 인자는 일단 addr의 크기를 설정하고(input) 접속이 완료되면
        // 실제로 addr에 설정된 접속한 client의 주소 정보의 크기를 저장
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        // accept 함수에서 clientaddr에 저장한 client 주소를 getnameinfo 함수 인자로 넣어
        // 서버의 ip 주소와 port 번호를 얻고 이를 출력함
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        // client 에서 받은 요청을 처리하는 doit 함수 진행
        doit(connfd);
        echo(connfd);
        Close(connfd);
    }
}

// doit 함수는 1개의 HTTP 트랜잭션 처리f
// 즉, 1개의 client 요청을 처리해 client에게 컨텐츠 제공
// 1. client의 HTTP 요청에서 요청 라인만 읽음 (Rio_readlineb을 통해 요청 텍스트의 제일 위 한줄(요청 라인)을 읽음)
// 요청 라인 -> GET/ HTTP /1.1 (method, uri, 요청이 준수하는 http 버전)
// 2. GET 메소드인지 확인(Tiny 서버에서는 최소한의 기능만 충족하도록 GET 메소드만 지원하므로!)
// 3. 요청 헤더는 사용하지 않을 것이기 때문에 읽고 무시
// 4. uri를 분해하여, uri, filename, cigiargs로 나누고 client가 정적 컨텐츠를 원하는지 동적 컨텐츠를 원하는지 확인
// 5. 실행을 원하는 파일의 stat 구조체의 st_mode를 이용해 파일이 읽기 권한과 실행 권한이 있는지 확인

void doit(int fd) {
    int is_static;
    struct stat sbuf;
    
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    // read request line and headers
    Rio_readinitb(&rio, fd);
    
    Rio_readlineb(&rio, buf, MAXLINE); //request line, header 읽기
    printf("Request headers:\n");
    printf("%s", buf);
    //buf에서 공백문자로 구분된 문자열 3개 읽어 각자 method, uri, version에 저장 (method, uri, 요청이 준수하는 http 버전)
    sscanf(buf, "%s %s %s", method, uri, version); 
    
    //GET요청인지 확인
    if (strcasecmp(method, "GET")) { //strcasecmp - 대소문자 구분없이 문자열 비교 함수
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }

    // 요청 헤더는 무시
    read_requesthdrs(&rio);

    // URI를 분해하여 URI, filename, cgiargs로 나눔
    // parse URI from GET request
    is_static = parse_uri(uri, filename, cgiargs);
    // stat (파일명 또는 파일 상대/절대 경로, 파일 상태 및 정보를 저장할 buf 구조체)
    // 즉 stat 구조체는, 파일의 정보를 저장하는 구조체로 stat(파일 이름, 정보를 저장할 주소) 함수 실행을 통해 얻을 수 있음

    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    } // 해당 filename이 유효한지 확인
    
    // 정적 컨텐츠 
    if (is_static) { //serve static content
        // 실행 가능한지 확인하는 조건문 -> 일반 파일인지, 읽기 권한을 갖고 있는지 확인
        // S_ISREG -> isregular : 일반 파일인지 확인하는 macro
        // st_mode는 파일의 유형값으로 bit& 연산으로 파일의 유형 확인 가능
        // S_IRUSR -> 읽기 권한이 있는지 
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    // 동적 컨텐츠
    // S_ISREG -> isregular : 일반 파일인지 확인하는 macro
    // S_IXUSR -> 실행 권한이 있는지
    else { //serve dynamic content
        if (!(S_ISREG(sbuf.st_mode)) || ! (S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}


// 일부 명백한 에러에 대해 client에게 HTTP 응답 보냄
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    // build the HTTP response body
    sprintf(body, "<html><title>Tiny Error</title>"); // body에 문자열 저장
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body); // 기존 body + 문자열 내용 -> body에 저장
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg); 
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    //print the HTTP response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

// tiny 웹서버는 요청 헤더 내의 어떤 정보도 사용하지 않음
// 따라서 요청 헤더를 종료하는 빈 텍스트줄(\r\n)이 나올 때까지 요청 헤더를 모두 읽어들임
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) { //strcmp 문자열 비교 // buf 내 빈 문자열이 있을 때까지
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

// parse_uri) URI 분해하는 함수 -> domain, path, cgiargs로 나눔
// cgiargs는, 동적 컨텐츠의 실행 파일에 들어갈 인자
// tiny 서버의 모든 동적 컨텐츠를 위한 실행파일은 cgi-bin이라는 디렉토리에 넣는 고전 방식으로 정적 컨텐츠와 분리시킴
// 따라서 URI에 cgi-bin이라는 경로가 있는지 확인하여 정적컨텐츠와 동적컨텐츠 중 어떤 것을 보낼 것인지 판단 가능

int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    if(!strstr(uri, "cgi-bin")) { // uri에 cgi-bin과 일치하는 문자열이 없으면 //static content
        //strstr : 문자열 내에서 문자열로 검색하기 //strcpy : 문자열 복사 //strcat : 문자열 결합
        strcpy(cgiargs, ""); 
        // cgiargs에 빈 문자열 저장(기존 CGI 인자 스트링 지우기)
        strcpy(filename, "."); // 아래 줄과 더불어 상대 리눅스 경로이름으로 변환(./index.html)
        strcat(filename, uri);
        if (uri[strlen(uri)-1] == '/') // uri가 '/' 문자로 끝난다면 기본 파일 이름 추가
            strcat(filename, "home.html");
        return 1;
    }
    else { // uri 내 "cgi-bin"이 있으면 //dynamic content
    // index() : 문자열 중 문자 위치 찾기 함수
        ptr = index(uri, '?');
        if (ptr) { //모든 cgi 인자 추출
            strcpy(cgiargs, ptr+1); // 포인터는 문자열 마지막으로 변경
            *ptr = '\0'; //uri 물음표 뒤 모두 제거
        }
        else // 
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}


// 서버의 정적 컨텐츠(디스크 파일) 처리
// 1. 컨텐츠 전달 전, 전달할 컨텐츠의 내용 및 크기 등 내역을 포함한 response header를 보냄
// 2. 요청한 파일을 읽기 전용으로 열고 파일의 내용을 가상메모리 영역에 저장
// 3. 가상메모리에 저장된 내용을 client와 연결된 연결식별자에 작성하여 컨텐츠를 client에게 보냄

void serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    //send response headers to client
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf); //빈줄 한 개 나오면 헤더 종료
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    //Send response body to client
    // O_RDONLY -> 파일을 읽기 전용으로 열기  // O_WRONLY -> 파일을 쓰기 전용으로 열기 // O_RDWR -> O_RDONLY와 O_WRONLY 합치기
    // 1) mmap version (read(file) + Malloc)
    // srcfd = Open(filename, O_RDONLY, 0); //파일 열기
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //가상메모리에 매핑
    // Close(srcfd);
    // Rio_writen(fd, srcp, filesize); //fd는 클라이언트에 전달용
    // Munmap(srcp, filesize);

    // Mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
    // fd로 지정된 파일에서 offset을 시작으로 length 바이트 만큼 start주소로 대응시키도록 함
    // start 주소는 단지 그 주소를 사용했으면 좋겠다는 정도 이므로 보통 0으로 지정
    // mmap는 지정된 영역에 대응된 실제 시작위치를 반환
    // prot 인자는 원하는 메모리 보호모드(:12)를 설정
    // -> PROT_EXEC - 실행가능, PROT_READ - 읽기 가능, NONE- 접근 불가, WRITE - 쓰기 가능d
    // flags는 대응된 객체의 타입, 대응 옵션, 대응된 페이지 복사본에 대한 수정이 그 프로세스에서만 보일건지, 다른 참조하는 프로세스와 공유할건지 설정
    // MAP_FIXED - 지정한 주소만 사용, 사용 못할 경우 실패
    // MAP_SHARED - 대응된 객체를 다른 모든 프로세스와 공유
    // MAP_PRIVATE - 다른 프로세스와 대응 영역 공유하지 않음

    // int munmap(void *addr, size_t len);
    // munmap 함수는 addr이 가리키는 영역에 len 크기만큼 할당하여 매핑한 메모리를 해제함
    // 매핑된 가상메모리 주소 반환
    // 파일을 메모리에 매핑한 후 더이상 이 식별자는 필요하지 않으므로 이 파일을 닫아줘야함(메모리 누수 방지)
    // mmap는 요청한 파일을 가상메모리 영역으로 매핑함

    // 2) malloc version
    srcfd = Open(filename, O_RDONLY, 0); //파일 열기
    srcp = (char*)malloc(filesize); //mmap 대신 malloc으로 할당한 메모리 반환해주기 //filesize는 byte 이므로
    Rio_readn(srcfd, srcp, filesize); // srcp는 buf 역할
    Close(srcfd);
    Rio_writen(fd, srcp, filesize); //fd는 클라이언트에 전달용
    // 파일을 client에게 전송 -> 주소 srcp에서 시작하는 filesize 바이트를 클라이언트의 연결 식별자로 복사
    free(srcp); //할당한 메모리 free처리
    
    
}
// response header에 들어갈 내용인 클라이언트가 요청한 파일의 타입을 확인
// get_filetype - Derive file type from filename
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4"); //비디오 파일 확인
    else
        strcpy(filetype, "text/plain");
}

// 동적컨텐츠 처리
// 1. 부모는 메모리를 물려받는 자식 프로세스를 만들고, 새로운 프로세스를 실행할 준비
// 2. exec는 fork와 달리 메모리를 물려받지 않기 때문에 전달하고 싶은 변수는 환경변수로 저장해야함
//    따라서, cgiargs를 query_string이라는 환경변수에 저장
// 3. client와 연결된 connfd를 표준 출력으로 재설정
//    이를 통해 CGI 프로그램이 표준 출력에 쓰는 모든 것은 직접 client 프로세스로 전달됨
// 4. 실행파일(CGI 프로그램)을 새로운 프로세스로 실행. 이때 마지막 인자로 environ을 넣어주면 프로그램 내에서
//    getenv 함수를 통해 기존 설정한 query_string 변수 사용 가능

void serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXLINE], *emptylist[] = { NULL };

    //return first part of HTTP response
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    // fork 함수를 호출하는 프로세스는 부모 프로세스가 되고, 새로 생성되는 프로세스는 자식 프로세스가 됨
    // fork 함수에 의해 생성된 자식 프로세스는 부모 프로세스의 메모리를 그대로 복사하여 갖게 됨
    // fork 함수 호출 이후 코드부터 각자의 메모리를 사용해 실행
    // fork()의 반환값 = 부모는 자식프로세스의 PID(프로세스아이디)값, 자식프로세스는 0
    // fork()의 값을 어디 변수에 저장해 놓으면 조건문으로 부모와 자식 프로세스에서 원하는 것 따로 실행 가능
    // pid_t pid = fork()/ if (pid > 0) (=부모){~~} else if (pid == 0) (=자식) {~~}

    if (Fork() == 0) { // 자식 프로세스  //child
        // real server would set all CGI vars here
        
        setenv("QUERY_STRING", cgiargs, 1);
        // setenv(const char* name, const char* value, int overwrite)
        // 환경변수 "name"을 현재의 환경 리스트에 삽입 또는 재설정
        // overwrite가 0이면 재설정되지 않음, 그 외의 경우 주어진 값에 재설정됨

        // dup2(fd, fd2) = fd의 값을 fd2로 지정함-> connfd를 STDOUT_FILENO로 바꿈(연결 바꿈)
        Dup2(fd, STDOUT_FILENO);
        //redirect stdout to client -> 프로세스가 로드되기 전 표준 출력을 클라이언트와 연관된 연결식별자로 재지정
        // 1) 자식은 자신의 표준 출력을 연결 파일 식별자로 재지정하고


        // 경로 또는 파일 이름으로 지정한 실행 파일을 실행하여 프로세스 생성
        // 부모와 자식 다른 작업하며 양쪽 모두 살아있도록 하기 위해 사용
        // execve(실행파일 or 명령어, argv(맨 뒤를 NULL로 넣어줘야 함(argc를 전달할 수 없기 때문)), 전달할 환경변수 - environ에 넣으면 기존에 설정한 환경변수 사용)
        Execve(filename, emptylist, environ); // Run CGI program
        // exec 호출하면 명령줄 인수, 환경변수만 전달받음
        // exec 호출하면 코드 영역에 있는 내용을 지우고, 새로운 코드로 바꿈
        // 또한 데이터 영역이 새로운 변수로 채워지고 스택 영역이 리셋됨

        // 2) 자식의 연결 파일 식별자 재지정 후, CGI 프로그램을 로드하고 실행
        // 3) CGI 프로그램이 자식 컨텍스트에서 실행되기 때문에 execve 함수를 호출하기 전에 존재하던 열린 파일들과
        //    환경변수에도 동일하게 접근 가능. 그래서 CGI 프로그램이 표준 출력에 쓰는 모든 것은 
        //    직접 클라이언트 프로세스로 부모 프로세스의 어떤 간섭도 없이 전달됨
    
    }
    Wait(NULL); //parent waits for and reaps child 자식프로세스가 종료되어 정리되기 기다림
}

void echo(int connfd) {
  size_t n;
  char buf[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, connfd);
  while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    printf("server received %d bytes\n", (int)n);
    Rio_writen(connfd, buf, n);
  }
}
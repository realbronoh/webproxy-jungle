#include "csapp.h"

int main(int argc, char **argv){
    int n;
    rio_t rio;
    char buf[MAXLINE];

    Rio_readinitb(&rio, STDIN_FILENO);
    while ((n = Rio_readnb(&rio, buf, 6)) != 0)
        Rio_writen(STDOUT_FILENO, buf, n);
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void){
    char content[200];

    strcpy(content, "hello");
    /* Generate the HTTP response */
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\n\r\n");
    printf("hello world!");
    // fflush(stdout);

    exit(0);
}
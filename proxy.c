#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define TINY_HOST_NAME "localhost"
#define TINY_PORT "8000"
// static char tiny_port[MAXLINE];

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse(char *uri, char *host, char *port);
void serve_static(int fd, int proxyfd, char *filename, int filesize, char method[MAXLINE]);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char method[MAXLINE]);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void transfer_request(rio_t *rp, int fd, int proxyfd);
void response_request(rio_t *rp, int fd, int proxyfd);

void doit(int fd)
{
  printf("\n--------------proxy doit start--------------\n");
  int is_static;
  int proxyfd;
  struct stat sbuf;
  char total_buf[MAXBUF], buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], host[MAXLINE], port[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t r;
  rio_t rio;
  rio_t rio2;

  strcpy(total_buf, "");
  Rio_readinitb(&r, fd);
  Rio_readlineb(&r, buf, MAXLINE);
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("uri = %s\n", uri);
  parse(uri, host, port);
  printf("uri = %s, host = %s, port = %s\n", uri, host, port);

  proxyfd = Open_clientfd(TINY_HOST_NAME, port);
  Rio_readinitb(&rio, fd);
  Rio_readinitb(&rio2, proxyfd);

  printf("\n--------------proxy transfer_request start--------------\n");
  printf("\nmethod = %s, uri = %s, version =%s\n", method, uri, version);
  // parse(uri, host, port);
  printf("uri = %s, host = %s, port = %s\n", uri, host, port);
  // printf("\nIn request, total = \n%s\n", total_buf);
  // printf("\nmethod = %s, uri = %s, version =%s\n", method, uri, version);
  strcpy(total_buf, method);
  strcat(total_buf, " ");
  strcat(total_buf, uri);
  strcat(total_buf, " ");
  strcat(total_buf, version);
  strcat(total_buf, "\r\n\r\n");
  printf("total_buf = %s\n", total_buf);
  Rio_writen(proxyfd, total_buf, strlen(total_buf));
  printf("\n--------------proxy transfer_request end----------------\n");

  // transfer_request(&rio, fd, proxyfd);
  response_request(&rio2, fd, proxyfd);
  Close(proxyfd);

  printf("\n--------------proxy doit end----------------\n");
}
void transfer_request(rio_t *rp, int fd, int proxyfd)
{
  printf("\n--------------proxy transfer_request start--------------\n");
  char total_buf[MAXBUF];
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], host[MAXLINE], port[MAXLINE];

  int idx = 0;
  printf("\n--------------proxy transfer_request loop start--------------\n");
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    if (idx == 0)
    {
      strcpy(total_buf, buf);
      printf("%s", buf);
      sscanf(buf, "%s %s %s", method, uri, version);
      idx++;
    }
    else
    {
      strcat(total_buf, buf);
      printf("%s", buf);
    }
  }
  printf("\n--------------proxy transfer_request loop end--------------\n");
  printf("\nmethod = %s, uri = %s, version =%s\n", method, uri, version);
  parse(uri, host, port);
  printf("uri = %s, host = %s, port = %s\n", uri, host, port);
  printf("\nIn request, total = \n%s\n", total_buf);
  printf("\nmethod = %s, uri = %s, version =%s\n", method, uri, version);
  strcpy(total_buf, method);
  strcat(total_buf, " ");
  strcat(total_buf, uri);
  strcat(total_buf, " ");
  strcat(total_buf, version);
  strcat(total_buf, "\r\n\r\n");
  printf("total_buf = %s\n", total_buf);
  Rio_writen(proxyfd, total_buf, strlen(total_buf));
  printf("\n--------------proxy transfer_request end----------------\n");
  return;
}
void response_request(rio_t *rp, int fd, int proxyfd)
{
  printf("\n--------------proxy response_request start--------------\n");
  char total_buf_2[MAXBUF];
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];

  int idx = 0;
  char size_str[MAXLINE];
  int size_i;
  // char *total_buf = (char *)calloc(MAXBUF, sizeof(char));
  printf("\nIn reponse, total0 = \n%s", total_buf_2);
  strcpy(total_buf_2, "");
  strcpy(buf, "");
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    if (strstr(buf, "Content-length:") != NULL)
    {
      char *ptr = strchr(buf, ':');
      strcpy(size_str, ptr + 1);
      // printf("size_str = %s\n", size_str);
    }
    if (idx == 0)
    {
      strcpy(total_buf_2, buf);
      printf("a%s", buf);
      // sscanf(buf, "%s %s %s", method, uri, version);
      idx++;
    }
    else
    {
      strcat(total_buf_2, buf);
      printf("a%s", buf);
    }
  }

  size_i = atoi(size_str);
  printf("size_i = %d\n", size_i);
  /*
  while (Rio_readlineb(rp, buf, MAXLINE))
  {
    if (idx == 0)
    {
      strcpy(total_buf2, buf);
      idx++;
    }
    else
    {
      strcat(total_buf2, buf);
    }
    printf("%s", buf);
  }
  */
  printf("\nIn reponse, total = \n%s", total_buf_2);
  printf("strlen(total_buf) = %d\n", strlen(total_buf_2));
  // Rio_writen(fd, total_buf_2, strlen(total_buf_2));

  char *srcp = (char *)calloc(1, size_i);
  rio_readnb(rp, srcp, size_i);
  printf("srcp = \n%s\n", srcp);
  Rio_writen(fd, srcp, size_i);
  free(srcp);
  // free(total_buf);
  printf("\n--------------proxy response_request end----------------\n");
  return;
}
int parse(char *uri, char *host, char *port)
{

  char *ptr1;
  char *ptr2;
  char *ptr3;

  ptr1 = strstr(uri, "http://");
  ptr1 += strlen("http://");
  ptr2 = strchr(ptr1, ':');
  ptr3 = strchr(ptr1, '/');

  // printf("ptr1 = %s\n", ptr1);
  // printf("ptr2 = %s\n", ptr2);
  // printf("ptr3 = %s\n", ptr3);

  // printf("ptr1 len = %d\n", strlen(ptr1));
  // printf("ptr2 len = %d\n", strlen(ptr2));
  // printf("ptr3 len = %d\n", strlen(ptr3));

  strncpy(host, ptr1, strlen(ptr1) - strlen(ptr2));
  printf("host = %s\n", host);

  strncpy(port, ptr2 + 1, strlen(ptr2) - strlen(ptr3) - 1);
  printf("port = %s\n", port);

  strcpy(uri, ptr3);
}

int main(int argc, char **argv)
{
  printf("%s", user_agent_hdr);
  // return 0;
  int listenfd, proxyfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  printf("\nargv[0] = %s\n", argv[0]);
  printf("argv[1] = %s\n", argv[1]);
  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

// int main() {
//   printf("%s", user_agent_hdr);
//   return 0;
// }

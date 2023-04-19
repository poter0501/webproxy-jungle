#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define TINY_HOST_NAME "localhost"
#define TINY_PORT "8000"

struct block
{
  char *alloc_ptr;
  char uri[MAXLINE];
  size_t body_size;
  struct block *pre;
  struct block *nxt;
} typedef block_t;

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
char *response_request(rio_t *rp, int fd, int proxyfd, char uri[MAXLINE]);
char *response_request_from_proxy(rio_t *rp, int fd, int proxyfd, block_t *target);

void *thread(void *vargp);
/* for cache */
block_t *new_block(char *alloc_ptr, size_t body_size, block_t *pre, block_t *nxt, char uri[MAXLINE]);
block_t *search(char target_uri[MAXLINE]);
void print_list();
void pop_list(block_t *target);
void add_list(block_t *target);
static block_t *head;
static block_t *tail;
/* --------- */

void print_list()
{
  printf("\n--------------proxy print_list start--------------\n");
  block_t *curr = head;
  while (curr != tail)
  {
    printf("curr(%x): alloc_ptr=%x, body_size=%d, pre=%x, nxt=%x\n",
           curr, curr->alloc_ptr, curr->body_size, curr->pre, curr->nxt);
    curr = curr->nxt;
  }
  printf("curr(%x): alloc_ptr=%x, body_size=%d, pre=%x, nxt=%x\n",
         curr, curr->alloc_ptr, curr->body_size, curr->pre, curr->nxt);
  curr = curr->nxt;
  printf("\n--------------proxy print_list end----------------\n");
}
void doit(int fd)
{
  int is_static;
  int proxyfd;
  struct stat sbuf;
  char total_buf[MAXBUF], buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], host[MAXLINE], port[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t r;
  rio_t rio;
  rio_t rio2;
  /* 요청 header를 tiny 서버에 넘겨주는 부분 */
  strcpy(total_buf, "");
  Rio_readinitb(&r, fd);
  Rio_readlineb(&r, buf, MAXLINE);
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  parse(uri, host, port);

  strcpy(total_buf, method);
  strcat(total_buf, " ");
  strcat(total_buf, uri);
  strcat(total_buf, " ");
  strcat(total_buf, version);
  strcat(total_buf, "\r\n\r\n");

  /* 캐싱된 리스트를 찾아서 이미 저장되어 있다면 굳이 tiny 서버에 연결하지 않고,
  저장되어 있지 않다면 연결해서 요청을 전달하고 응답을 받아 클라이언트에게 응답을 전달 */
  block_t *target = search(uri);

  if (target == NULL) /* not yet cached contents */
  {
    proxyfd = Open_clientfd(TINY_HOST_NAME, port);
    Rio_readinitb(&rio, fd);
    Rio_readinitb(&rio2, proxyfd);

    printf("Miss !\n");
    Rio_writen(proxyfd, total_buf, strlen(total_buf));
    response_request(&rio2, fd, proxyfd, uri);

    Close(proxyfd);
  }
  else /* already cached contents */
  {
    printf("Hit !\n");
    response_request_from_proxy(&rio2, fd, proxyfd, target);
  }
}
char *response_request(rio_t *rp, int fd, int proxyfd, char uri[MAXLINE])
{
  printf("\n--------------proxy response_request start--------------\n");
  char total_buf_2[MAXBUF];
  char buf[MAXLINE], method[MAXLINE], version[MAXLINE];

  int idx = 0;
  char size_str[MAXLINE];
  int size_i;
  /* Header 정보를 받아오는 부분 */
  strcpy(total_buf_2, "");
  strcpy(buf, "");
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    if (strstr(buf, "Content-length:") != NULL)
    {
      char *ptr = strchr(buf, ':');
      strcpy(size_str, ptr + 1);
    }
    if (idx == 0)
    {
      strcpy(total_buf_2, buf);
      printf("%s", buf);
      idx++;
    }
    else
    {
      strcat(total_buf_2, buf);
      printf("%s", buf);
    }
  }

  size_i = atoi(size_str);

  Rio_writen(fd, total_buf_2, strlen(total_buf_2));
  char *srcp = (char *)calloc(1, size_i);
  rio_readnb(rp, srcp, size_i);

  /* 캐싱 하는 부분 */
  if (size_i <= MAX_OBJECT_SIZE)
  {
    /* 캐싱할 body의 정보를 담을 공간을 할당하고,
    기존에 버퍼에 있던 정보를 새롭게 할당받은 공간으로 복사.
    그 이후 block 구조체를 사용해서 캐싱된 리스트에 삽입.
    만약 캐쉬의 최대 사이즈를 초과하면 뒤에서 부터 삭제 */
    size_t curr_cache_size = head->body_size;
    char *new_alloc_ptr = (char *)calloc(1, size_i);
    memcpy(new_alloc_ptr, srcp, size_i);
    block_t *created_block = new_block(new_alloc_ptr, size_i, NULL, NULL, uri);

    if (curr_cache_size + size_i <= MAX_CACHE_SIZE)
      add_list(created_block);
    else /* cache 사이즈를 초과해서 저장해야 하는 경우, 리스트의 뒤부터 삭제 */
    {
      block_t *will_deleted_block = tail->pre;
      while (will_deleted_block != head)
      {
        if (head->body_size + size_i <= MAX_CACHE_SIZE)
        {
          add_list(created_block);
          break;
        }
        else
        {
          block_t *will_deleted_block_pre = will_deleted_block->pre;
          pop_list(will_deleted_block);
          will_deleted_block_pre = will_deleted_block_pre;
        }
      }
    }
  }

  Rio_writen(fd, srcp, size_i);
  free(srcp);
  return;
}
char *response_request_from_proxy(rio_t *rp, int fd, int proxyfd, block_t *target)
{
  char filetype[MAXLINE], buf[MAXBUF];
  char filename[MAXLINE];
  strcpy(filename, target->uri);
  printf("filename = %s\n", filename);
  size_t filesize = target->body_size;
  /* create header */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  /* read cache and tranfer cached content */
  printf("cache write act before");
  Rio_writen(fd, target->alloc_ptr, filesize);
  printf("body = \n%s\n", target->alloc_ptr);
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

  strncpy(host, ptr1, strlen(ptr1) - strlen(ptr2));
  printf("host = %s\n", host);

  strncpy(port, ptr2 + 1, strlen(ptr2) - strlen(ptr3) - 1);
  printf("port = %s\n", port);

  strcpy(uri, ptr3);
}

/*
 * get_filetype - Derive file type from filename
 */
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
    strcpy(filetype, "image/mp4");
  else
    strcpy(filetype, "text/plain");
}
block_t *new_block(char *alloc_ptr, size_t body_size, block_t *pre, block_t *nxt, char uri[MAXLINE])
{
  block_t *new_blck = (block_t *)calloc(1, sizeof(block_t));
  new_blck->alloc_ptr = alloc_ptr;
  new_blck->body_size = body_size;
  new_blck->pre = pre;
  new_blck->nxt = nxt;
  strcpy(new_blck->uri, uri);
  return new_blck;
}
block_t *search(char target_uri[MAXLINE])
{
  if (head->nxt == tail)
  {
    return NULL;
  }

  block_t *curr = head->nxt;
  while (curr != tail)
  {
    if (strcmp(curr->uri, target_uri) == 0)
    {
      pop_list(curr);
      add_list(curr);
      return curr;
    }
    else
      curr = curr->nxt;
  }
  return NULL;
}
void pop_list(block_t *target)
{
  block_t *previous = target->pre;
  block_t *next = target->nxt;

  previous->nxt = next;
  next->pre = previous;

  target->pre = NULL;
  target->nxt = NULL;

  head->body_size -= target->body_size;
  tail->body_size -= target->body_size;
}
void add_list(block_t *target)
{
  block_t *first = head->nxt;

  head->nxt = target;
  target->pre = head;
  target->nxt = first;
  first->pre = target;

  head->body_size += target->body_size;
  tail->body_size += target->body_size;
}
int main(int argc, char **argv)
{
  int listenfd, *connfdp;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  /* initialize tha head and tail of cache list */
  head = new_block(NULL, 0, NULL, NULL, "");
  tail = new_block(NULL, 0, NULL, NULL, "");
  head->nxt = tail;
  tail->pre = head;

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(struct sockaddr_storage);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Pthread_create(&tid, NULL, thread, connfdp);
  }
}

/* Thread routine */
void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

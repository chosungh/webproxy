/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <_strings.h>
// #include <cstdio>
#include <stdio.h>
#include <sys/_types/_s_ifmt.h>

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
char *shortmsg, char *longmsg);

void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];

  // rio는 Robust I/O의 줄임말로, 안정적인 입출력을 지원하는 구조체.
  // rio_t 구조체 rio 선언
  
  // rio_t 구조체
  // typedef struct {
  //     int rio_fd;                구조체 안에서 파일 디스크립터를 저장하는 변수
  //     int rio_cnt;               내부 버퍼에 남아있는 아직 읽히지 않은 바이트 수 (아직 프로그램이 읽지 않음)
  //     char *rio_bufptr;          내부 버퍼에서 다음에 읽어야 할 바이트의 위치
  //     char rio_buf[RIO_BUFSIZE]; 크기가 RIO_BUFSIZE인 내부 버퍼를 선언
  // } rio_t;
  rio_t rio;

  // Rio_readinitb(&rio, fd);는 
  // fd(소켓 파일 디스크립터)와 rio(버퍼 구조체)를 연결하고 내부 상태를 초기화해,
  // 이 함수가 호출된 이후 rio 구조체를 이용해 해당 fd의 데이터를 안전하게 읽어올 수 있게 준비해주는 역할을 한다.
  Rio_readinitb(&rio, fd);

  // 클라이언트가 보낸 요청(예: "GET /index.html HTTP/1.1")을 읽어서 buf에 저장한다.
  // 클라이언트가 보낸 요청라인을 소켓에서 읽어서 buf 배열에 복사한다.
  Rio_readlineb(&rio, buf, MAXLINE);
  
  // 클라이언트가 보낸 요청 확인
  printf("Request headers: \n");
  printf("%s", buf);

  // buf에 저장된 클라이언트의 첫 번째 요청을 나눈다
  // 각각 method(HTTP 메서드), uri(요청된 경로), version(HTTP 버전) 변수에 저장한다.
  // 예) 첫번째 요청 "GET /index.html HTTP/1.0\r\n"을 공백을 기준으로 파싱. 
  // 결과) method = GET, uri = /index.html, version = HTTP/1.0
  sscanf(buf, "%s %s %s", method, uri, version);

  // 이 조건문은 클라이언트가 보낸 HTTP 메서드가 "GET"이 아니면 에러를 응답.
  // why? Tiny는 GET 요청만을 지원한다. 따라서 다른 요청 POST, DELETE 등을 요청하면 에러 메세지 반환
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  // read_requesthdrs(&rio);
  // 클라이언트가 보내온 HTTP 요청 헤더들을 한 줄씩 읽어서 출력하거나 무시.
  // 요청 헤더의 마지막은 빈 줄("\r\n" 또는 "\n")로 표시되기에  \n을 만나기 전까지 모두 읽음
  read_requesthdrs(&rio);

  
  // parse_uri 함수는 uri를 분석해서 정적 컨텐츠 요청인지, 동적 컨텐츠 요청인지 판단해서 is_static에 결과 저장
  is_static = parse_uri(uri, filename, cgiargs);

  // 만약 filename에 해당하는 파일이 실제로 존재하지 않는다면, 클라이언트에게 "404" 에러를 보내고 이후 처리를 중단한다.
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  // 정적 파일을 요청 받았을 때
  if (is_static) {
    // sbuf.st_mode는 파일의 타입 + 권한 정보를 가지고 있는 정수 값이다. 
    // S_ISREG(sbuf.st_mode): sbuf.st_mode가 일반 파일인지 확인
    // S_IRUSR & sbuf.st_mode: 해당 파일에 대해 사용자가 읽기 권한이 있는지 확인
    // 어떻게? &연산을 통해 읽기 권한 비트(0400)가 켜져 있는지 검사
    // 둘 중 하나라도 아니면 서버가 파일을 읽을 수 없으므로 403 에러를 클라이언트에 보낸다.
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 위 조건 모두 통과 시, 정적 파일을 클라이언트에게 제공한다.
    serve_static(fd, filename, sbuf.st_size);
  }
  // 동적 파일을 요청 받았을 때
  else {
    // S_ISREG(sbuf.st_mode): sbuf.st_mode가 일반 파일인지 확인
    // S_IXUSR & sbuf.st_mode: 해당 파일에 대해 사용자가 실행 권한이 있는지 확인
    // 어떻게? 이 부분은 권한 설정을 어떻게 하는지 그 형식에 대해 공부하기
    // 둘 중 하나라도 아니면 서버가 파일을 실행할 수 없으므로 403에러를 클라이언트에 보낸다. 
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 위 조건을 모두 통과 시, 정적 파일을 클라이언트에게 제공한다.
    serve_dynamic(fd, filename, cgiargs);
  }
}


// 클라이언트가 보낸 요청 헤더들을 읽는 함수
void read_requesthdrs(rio_t *rp) {
 
  // 요청 헤더를 저장할 버퍼 선언(8192바이트)
  char buf[MAXLINE];

  // 클라이언트가 보낸 첫 번째 요청(예: "GET /index.html HTTP/1.1")을 읽어서 buf에 저장한다.
  Rio_readlineb(rp, buf, MAXLINE);
  
  // 클라이언트가 보낸 요청 헤더들을 읽으면서, 한 줄씩 출력.
  while(strcmp(buf, "\r\n")) {       // buf에 저장된 헤더 한 줄이 "\r\n" (빈 줄)이 아닐 때까지 반복
    Rio_readlineb(rp, buf, MAXLINE); // 다음 줄의 헤더를 읽어서 buf에 저장
    printf("%s", buf);               // 방금 읽은 헤더 줄을 출력
  }
  return;
}

// uri가 정적인지 동적인지 확인
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  // 입력받은 uri가 정적 컨텐츠인지 확인
  if (!strstr(uri, "cgi-bin")) {
    // GET /index.html HTTP/1.1 <- 여기서 "/index.html"이 uri

    // 쿼리 스트링을 비운다(cgiargs를 빈 문자열로 만듦)
    strcpy(cgiargs, "");  // cgiargs에 아무 인자도 없는 상태로 초기화

    // filename을 현재 디렉터리(.)로 시작하게 준비
    strcpy(filename, ".");  

    // filename 뒤에 uri(요청 경로)를 붙여서 실제 파일 경로를 만든다.
    // uri가 "/index.html"이면 filename은 "./index.html"이 된다.
    strcat(filename, uri);
  
  // 만약 uri가 '/'으로 끝난다면 뒤에 home.html을 이어 붙인다.
  if (uri[strlen(uri)-1] == '/') {
    strcat(filename, "home.html");
  }
  // 이게 정적 컨텐츠임을 나타내기 위해 1을 반환
  return 1;
  }
  else { // 입력받은 uri가 동적임

    // uri 문자열에서 쿼리 부분을 가리키게 함
    ptr = index(uri, '?'); 

    if (ptr) {      // 쿼리가 존재하는지 확인
      strcpy(cgiargs, ptr+1);   // '?'다음 문자열을 복사
      *ptr = '\0'; // '?' 자리에 널문자 삽입 ( \0 == null )
    }
    else // 쿼리가 존재하지 않음
      strcpy(cgiargs, ""); // 쿼리 스트링 비움
      strcpy(filename, "."); // 현재 디렉터리로 시작하게 준비
      strcat(filename, uri); // 파일 경로 완성
    return 0; // 이게 동적 컨텐츠임을 나타내기 위해 0을 반환
  }
}


// 클라이언트에게 정적 파일을 제공하는 함수
void serve_static(int fd, char *filename, int filesize) {
  int srcfd;    // fd 번호를 저장하기 위한 변수 선언
  char *srcp, filetype[MAXLINE], buf[MAXBUF];  
  
  // filename의 확장자에 따라 전달할 컨텐츠 타입 결정
  get_filetype(filename, filetype); 

  // HTTP 응답 헤더를 만들어 buf에 저장
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

  // buf를 클라이언트에 전송
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf); //buf 내용을 서버 콘솔에 출력

  // 읽기 전용(O_RDONLY)으로 파일 열고 그 파일에 대한 번호(파일 디스크립터)를 반환하여 저장
  srcfd = Open(filename, O_RDONLY, 0);
  
  // 파일을 프로세스 메모리에 매핑
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); 
  
  // 파일 디스크립터 닫기. 왜? -> 이미 메모리에 매핑되었기 때문(파일 디스크립터가 없어도 접근할 수 있기 때문)
  // close()는 파일 디스크립터 테이블에서 항목을 제거하지만, 
  // mmap()으로 매핑된 메모리는 여전히 프로세스의 가상 메모리 공간에 남아 있기 때문에 접근이 가능하다. 
  // 따라서 fd를 닫아도 매핑된 메모리를 통해 파일 내용을 읽을 수 있다.
  Close(srcfd);
  
  // 파일 내용을 클라이언트에 전송
  Rio_writen(fd, srcp, filesize);
  
  // 메모리 매핑 해제
  Munmap(srcp, filesize);
}

// 클라이언트에게 동적 파일을 제공하는 함수
void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };
  
  // http 응답 헤더를 만들어 buf에 저장
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // Fork()는 호출 시 현재 Tiny 서버 프로세스를 복제함. 
  // Fork()를 호출한 프로세스가 부모(자식의 PID를 받음)가 되고, 복제된 프로세스가 자식(PID가 0)임 
  if (Fork() == 0) { // Fork()는 PID를 2번(자식과 부모) 반환함. 따라서 우리는 자식 프로세스에 아래 코드를 실행함 
    
    // 브라우저에선 전달된 쿼리 스트링을 환경 변수 QUERY_STRING을 통해 받음
    setenv("QUERY_STRING", cgiargs, 1);

    // CGI 프로그램이 출력하는 모든 데이터가 클라이언트 브라우저로 직접 전송됨
    Dup2(fd, STDOUT_FILENO);

    // 이 프로세스의 코드 전체를 filename으로 바꿔라
    Execve(filename, emptylist, environ); 
  }
  Wait(NULL); // Wait을 호출하면 자식 프로세스가 사라짐
}


void get_filetype(char *filename, char *filetype)
  {
    // 파일 이름의 확장자를 확인해서 적절한 MIME 타입을 filetype에 저장한다.
    if (strstr(filename, ".html"))
      strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
      strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
      strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
      strcpy(filetype, "image/jpeg");
    else
      strcpy(filetype, "text/plain");
}

// Tiny 서버가 클라이언트에게 오류 응답을 보낼 때 사용하는 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  
  // buf: http 헤더 정보를 임시로 담는 버퍼
  // body: http 본문 정보를 임시로 담는 버퍼
  char buf[MAXLINE], body[MAXBUF];

  // sprintf(body, format)은 body에 format의 내용을 저장하겠다.
  // body에 "<html><title>Tiny Error</title>" 이 내용을 저장하겠다.
  sprintf(body, "<html><title>Tiny Error</title>");
  
  // body에 저장되어 있던 내용 뒤에 "<body bgco~lor=""ffffff"">\r\n" 이 내용을 저장하겠다.
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);

  // body에 저장되어 있던 내용 뒤에 에러 번호(ex. 404)와 짧은 에러 메시지(Not Found) 추가하여 저장
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);

  // body에 저장되어 있던 내용 뒤에 긴 에러 메시지(longmsg)와 직접적인 원인(cause)를 추가하여 저장
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);

  // body에 저장되어 있던 내용 뒤에 "<hr><em>The Tiny Web server</em>\r\n" 을 추가한다.
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // http
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

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

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}
#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>

struct HttpStartLine {
  char * method;
  char * path;
  char * protocol;
  int length;
};

struct Header {
  char * key;
  char * value;
};

struct Headers {
  struct Header * list;
  int length;
  int charLen;
};

void freeHeaders(struct Headers *h);
void freeHttpStartLine(struct HttpStartLine *sl);
struct HttpStartLine parseHttpData(char *rawHeaders);
struct Headers parseHeaders(char *rawHeaders, struct HttpStartLine *sl);

#endif

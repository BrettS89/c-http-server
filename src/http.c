#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "http.h"

#define HEADER_CAPACITY 10

void freeHeaders(struct Headers *h) {
  for (int i = 0; i < h->length; i++) {
    free(h->list[i].key);
    free(h->list[i].value);
  }

  free(h->list);
}

void freeHttpStartLine(struct HttpStartLine *sl) {
  free(sl->method);
  free(sl->path);
  free(sl->protocol);
}

struct HttpStartLine parseHttpData(char *rawHeaders) {
  int line = 0;
  int i = 0;

  char method[10];
  int methodLen = 0;

  char path[1024];
  int pathLen = 0;

  char protocol[1024];
  int protocolLen = 0;

  while(rawHeaders[i] != '\n') {
    if (rawHeaders[i] == ' ') {
      if (line == 0) method[methodLen] = '\0';
      else if (line == 1) path[pathLen] = '\0';
      else if (line == 2) protocol[protocolLen] = '\0';

      line++;
    } else if (line == 0) {
      method[methodLen] = rawHeaders[i];
      methodLen++;
    } else if (line == 1) {
      path[pathLen] = rawHeaders[i];
      pathLen++;
    } else if (line == 2) {
      protocol[protocolLen] = rawHeaders[i];
      protocolLen++;
    }

    i++;
  }

  struct HttpStartLine sl = {
    .length = i + 1,
    .method = strdup(method),
    .path = strdup(path),
    .protocol = strdup(protocol),
  };

  if (!sl.method || !sl.path || !sl.protocol) {
    free(sl.method);
    free(sl.path);
    free(sl.protocol);
    sl.length = 0;

    perror("Memory allocation failed for http start line");
  }

  return sl;
}

struct Headers parseHeaders(char *rawHeaders, struct HttpStartLine *sl) {
  int capacity = HEADER_CAPACITY;

  struct Headers headers = {
    .list = NULL,
    .length = 0,
    .charLen = 0,
  };

  headers.list = malloc(sizeof(struct Header) * capacity);

  if (!headers.list) {
    perror("Memory allocation failed for header list\n");
    return headers;
  }

  int i = sl->length;

  char keyBuffer[1024];
  int keyLen = 0;

  char valueBuffer[1024];
  int valueLen = 0;

  bool isKey = true;

  struct Header h = {0};

  while (rawHeaders[i] != '\0') {
    if (rawHeaders[i] == '\r') {
      valueBuffer[valueLen] = '\0';
      h.value = strdup(valueBuffer);
      h.key = strdup(keyBuffer);

      if (!h.value || !h.key) {
        free(h.value);
        free(h.key);
        freeHeaders(&headers);
        return headers;
      }

      headers.list[headers.length] = h;

      strcpy(keyBuffer, "");
      strcpy(valueBuffer, "");

      headers.length++;
      keyLen = 0;
      valueLen = 0;

      isKey = true;

      memset(&h, 0, sizeof(struct Header));

      if (rawHeaders[i + 1] == '\n' && rawHeaders[i + 2] == '\r') {
        headers.charLen = i + 1;
        break;
      }

      i++;
    } else {
      if ((int)rawHeaders[i] == 32) {
        isKey = false;
        keyBuffer[keyLen - 1] = '\0';
      } else {
        if (isKey) {
          keyBuffer[keyLen] = rawHeaders[i];
          keyLen++;
        } else {
          valueBuffer[valueLen] = rawHeaders[i];
          valueLen++;
        }
      }
    }

    i++;
  }

  free(h.key);
  free(h.value);

  return headers;
}

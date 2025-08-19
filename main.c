#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define HEADER_CAPACITY 10

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
};

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
  };

  headers.list = malloc(sizeof(struct Header) * capacity);

  if (!headers.list) {
    perror("Memory allocation failed for header list\n");
    return headers;
  }

  int i = sl->length;
  int j = 0;

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

int createSocketDesc() {
  short socketId = 0;
  socketId = socket(AF_INET, SOCK_STREAM, 0);
  return socketId;
}

int bindSocket(int socketId) {
  int port = 4001;
  int iRetval = -1;

  struct sockaddr_in remote = {0};

  remote.sin_family = AF_INET;
  remote.sin_addr.s_addr = htonl(INADDR_ANY);
  remote.sin_port = htons(port);

  iRetval = bind(socketId, (struct sockaddr *) &remote, sizeof(remote));

  return iRetval;
}

int main() {
  int socket = 0;
  int socketDesc = createSocketDesc();
  struct sockaddr_in client;

  if (socketDesc == -1) {
    perror("Could not create socket description");
    return 1;
  }

  if (bindSocket(socketDesc) < 0) {
    perror("Bind failed");
    return 1;
  }

  if (listen(socketDesc, 100) < 0) {
    perror("Listen failed");
    return 1;
  }

  while(1) {
    socklen_t clientLen = sizeof(struct sockaddr_in);

    printf("Waiting for incomming connections\n");
    int sock = accept(socketDesc, (struct sockaddr *)&client, (socklen_t*)&clientLen);

    if (sock < 0) {
      fprintf(stderr, "accept failed\n");
      return 1;
    }

    char *clientMessage = NULL;
    size_t bufferSize = 4096;
    size_t received = 0;
    size_t contentLength = 0;
    int bytes;

    clientMessage = malloc(bufferSize);

    if (!clientMessage) {
      perror("Memory allocation failed for clientMessage");
      close(sock);
      continue;
    }

    char *headerEnd = NULL;
    while(!headerEnd) {
      bytes = recv(sock, clientMessage + received, bufferSize - received - 1, 0);

      if (bytes <= 0) {
        if (bytes == 0) printf("Client disconnected during header recv\n");
        else perror("recv headers failed");
        free(clientMessage);
        close(sock);
        continue;
      }

      received+= bytes;
      clientMessage[received] = '\0';
      headerEnd = strstr(clientMessage, "\r\n\r\n");
    }

    printf("%s\n\n\n", clientMessage);

    struct HttpStartLine sl = parseHttpData(clientMessage);
    struct Headers headers = parseHeaders(clientMessage, &sl);

    for (int i = 0; i < headers.length; i++) {
      printf("%s: %s\n", headers.list[i].key, headers.list[i].value);
    }

    if (!headers.length) {
      close(sock);
      freeHeaders(&headers);
      freeHttpStartLine(&sl);
      return 1;
    }

    const char *body = "hi bro";
    size_t body_len = strlen(body);
    char response[1024];

    int response_len = snprintf(response, sizeof(response),
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: %zu\r\n"
      "\r\n"
      "%s", body_len, body);

    if (response_len < 0 || (size_t)response_len >= sizeof(response)) {
      perror("Response formatting failed");
      close(sock);
      freeHeaders(&headers);
      freeHttpStartLine(&sl);
      return 1;
    }

    ssize_t sent = send(sock, response, response_len, 0);

    if (sent < 0) {
      perror("Error sending http response");
      close(sock);
      freeHeaders(&headers);
      freeHttpStartLine(&sl);
      continue;
    }

    freeHeaders(&headers);
    freeHttpStartLine(&sl);
  }

  return 0;
}
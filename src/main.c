#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "http.h"

#define PORT 4000

int createSocketDesc() {
  short socketId = 0;
  socketId = socket(AF_INET, SOCK_STREAM, 0);
  return socketId;
}

int bindSocket(int socketId) {
  int iRetval = -1;

  struct sockaddr_in remote = {0};

  remote.sin_family = AF_INET;
  remote.sin_addr.s_addr = htonl(INADDR_ANY);
  remote.sin_port = htons(PORT);

  iRetval = bind(socketId, (struct sockaddr *) &remote, sizeof(remote));

  return iRetval;
}

int main() {
  signal(SIGPIPE, SIG_IGN);

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

    printf("Waiting for incoming connections on port %d\n", PORT);
    int sock = accept(socketDesc, (struct sockaddr *)&client, (socklen_t*)&clientLen);

    if (sock < 0) {
      fprintf(stderr, "accept failed\n");
      continue;
    }

    char *clientMessage = NULL;
    size_t bufferSize = 4096;
    size_t received = 0;
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

    if (!headers.length) {
      free(clientMessage);
      close(sock);
      freeHeaders(&headers);
      freeHttpStartLine(&sl);
      free(clientMessage);
      continue;
    }

    char *body = NULL;
    int lenWritten = 0;

    if (strcmp(sl.method, "POST") == 0 || strcmp(sl.method, "PATCH") == 0 || strcmp(sl.method, "PUT") == 0) {
      for (int i = 0; i < headers.length; i++) {
        if (strcmp(headers.list[i].key, "Content-Length") == 0) {
          int bodyLen = atoi(headers.list[i].value);
          if (bodyLen > 0) {

            body = malloc(bodyLen + 1);

            if (!body) {
              perror("Memory allocation failed for request body");
              close(sock);
              free(clientMessage);
              freeHeaders(&headers);
              freeHttpStartLine(&sl);
              continue;
            }

            int j = headers.charLen + 3;

            while (lenWritten < bodyLen) {
              if (clientMessage[j] == '\0') {
                recv(sock, clientMessage + received, bufferSize - received - 1, 0);
              }

              body[lenWritten] = clientMessage[j];
              j++;
              lenWritten++;
            }

            body[lenWritten] = '\0';
            
            printf("Request body: %s\n", body);
            printf("content length: %d\n", bodyLen);
            printf("body len: %lu\n", strlen(body));
            printf("len written: %d\n", lenWritten);
            break;
          }
        }
      }
    }
    
    const char *respBody = "{ \"message\": \"Hello from the C http server!\" }";
    size_t body_len = strlen(respBody);
    char response[1024];

    int response_len = snprintf(response, sizeof(response),
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %zu\r\n"
      "\r\n"
      "%s", body_len, respBody);

    if (response_len < 0 || (size_t)response_len >= sizeof(response)) {
      perror("Response formatting failed");
      close(sock);
      freeHeaders(&headers);
      freeHttpStartLine(&sl);
      free(body);
      free(clientMessage);
      continue;
    }

    ssize_t sent = send(sock, response, response_len, 0);

    if (sent < 0) {
      perror("Error sending http response");
      close(sock);
      freeHeaders(&headers);
      freeHttpStartLine(&sl);
      free(body);
      free(clientMessage);
      continue;
    }

    freeHeaders(&headers);
    freeHttpStartLine(&sl);
    free(body);
    free(clientMessage);
    close(sock);
  }

  return 0;
}

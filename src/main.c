#include <errno.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_MAX_SIZE 1024
#define RESPONSE_MAX_SIZE 1024*2
#define RES200 "HTTP/1.1 200 OK\r\n"
#define RES404 "HTTP/1.1 404 Not Found\r\n\r\n"

typedef struct {
    char *method;
    char *url;
    char *protocol;
    char *host;
    char *accept;
    char *userAgent;
} Request;

typedef struct {
    char content[BUFFER_MAX_SIZE];
    size_t size;
    char* path;
} File;

void handle_request(int client_fd);

File readFile(File* file)
{
    FILE *fp;

    fp = fopen(file->path, "r");
    if (fp == NULL) {
        printf("Unable to open file.\n");
        file->path = NULL;
        return *file;
    }

    fseek(fp, 0, SEEK_END);
    file->size = ftell(fp);
    rewind(fp);

    if (file->size >= BUFFER_MAX_SIZE) {
        printf("Buffer is too small to hold the file contents.\n");
        fclose(fp);
    }

    size_t bytesRead = fread(file->content, sizeof(char), file->size, fp);
    if (bytesRead != file->size) {
        printf("Error reading file: %zu bytes read instead of %zu\n", bytesRead, file->size);
    }

    file->content[file->size] = '\0'; // add null terminator

    fclose(fp);
    return *file;
}

File writeFile(File* file, char* body, int bodyLen)
{
    FILE *fp;

    fp = fopen(file->path, "w");
    if (fp == NULL) {
        printf("Unable to create the file.\n");
        file->path = NULL;
    }
    fwrite(body, 1, bodyLen, fp);
    return *file;
}

void handle_request(int client_fd)
{
    char buffer[BUFFER_MAX_SIZE];
    int bytes_request = recv(client_fd, buffer, BUFFER_MAX_SIZE - 1, 0);
    if (bytes_request < 0) {
        printf("Error --> %s\n", strerror(errno));
        close(client_fd);
        return;
    }

    Request req;
    char response[RESPONSE_MAX_SIZE];

    printf("Buffer-------------------------\n %s\n", buffer);

    req.method = strtok(buffer, " ");
    req.url = strtok(NULL, " ");
    req.protocol = strtok(NULL, "\r\n");
    req.host = strtok(NULL, "\r\n");

    printf("\nEnd of buffer -----------------\n");

    printf("Method --> %s\n", req.method);
    printf("Url --> %s\n", req.url);
    printf("Protocol --> %s\n", req.protocol);
    printf("Host --> %s\n\n", req.host);

    if (strcmp(req.url, "/") == 0) 
    {
        sprintf(response, "%s\r\n", RES200);
    }
    else if (strncmp(req.url, "/echo/", 6) == 0)
    {
        char *echoStr = req.url + 6;
        int echoLen = strlen(echoStr);

        sprintf(response, "%sContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", RES200, echoLen, echoStr);
    } 
    else if (strcmp(req.url, "/user-agent" ) == 0)
    {
        strtok(NULL, " ");
        req.userAgent = strtok(NULL, "\r\n");
        int userAgentLen = strlen(req.userAgent);

        sprintf(response, "%sContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", RES200, userAgentLen, req.userAgent);
    }
    else if (strncmp(req.url, "/files/", 7) == 0 && strcmp(req.method, "GET") == 0)
    {
        printf("Inside the files if statement.\n\n");

        File file;
        file.path = req.url + 7;

        file = readFile(&file);
        if (file.path == NULL) {
            sprintf(response, "%s", RES404);
        } else {
            sprintf(response, "%sContent-Type: application/octet-stream\r\nContent-Length: %zu\r\n\r\n%s", RES200, file.size, file.content);
        }

    }

    else if (strncmp(req.url, "/files/", 7) == 0 && strcmp(req.method, "POST") == 0)
    {
        File file;
        file.path = req.url + 7;

        strtok(NULL, " ");
        char* contentLen =strtok(NULL, "\r\n");
        strtok(NULL, "\r\n");
        char* body = strtok(NULL, "\r\n");

        printf("contentLen --> %s\n", contentLen);
        printf("body --> %s\n", body);

        int bodyLen = atoi(contentLen);
        file = writeFile(&file, body, bodyLen);

        if (file.path == NULL) {
            sprintf(response, "%s", RES404);
        } else {
            sprintf(response, "HTTP/1.1 201 Created\r\n\r\n");
        }

    }
    else
    {
        sprintf(response, "%s", RES404);
    }

    printf("Response --> %s\n", response);
    int bytes_sent = send(client_fd, response, strlen(response), 0);
    if (bytes_sent < 0) {
        printf("Error sending response: %s\n", strerror(errno));
    }
    close(client_fd);
}
int main(int argc, char *argv[]) 
{
    if (argc >= 3) {
        if (strcmp(argv[1], "--directory") == 0) {
            char *directory = argv[2];
            chdir(directory) < 0 ? printf("Failed to set current dir"), 1 : printf("Directory: %s\n", directory);
        }
    }

    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // You can use print statements as follows for debugging, they'll be visible
    // when running tests.
    printf("Logs from your program will appear here!\n");

    int server_fd, client_addr_len;
    struct sockaddr_in client_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return 1;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(4221),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (unsigned int *)&client_addr_len);
        printf("Client connected\n\n");
        if (fork() == 0) {
            close(server_fd);
            handle_request(client_fd);
            exit(0);
        } else {
            close(client_fd);
        }
    }
    return 0;
}

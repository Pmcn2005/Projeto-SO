#include "api.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "src/common/constants.h"
#include "src/common/io.h"
#include "src/common/protocol.h"

char req_pipe[40];
char resp_pipe[40];
char notif_pipe[40];

// variaveis globais para guardar os fd dos pipes
int req_pipe_fd;
int resp_pipe_fd;

int kvs_connect(char const *req_pipe_path, char const *resp_pipe_path,
                char const *server_pipe_path, char const *notif_pipe_path,
                int *notif_pipe_fd) {
    strncpy(req_pipe, req_pipe_path, sizeof(req_pipe) - 1);
    req_pipe[sizeof(req_pipe) - 1] = '\0';
    strncpy(resp_pipe, resp_pipe_path, sizeof(resp_pipe) - 1);
    resp_pipe[sizeof(resp_pipe) - 1] = '\0';
    strncpy(notif_pipe, notif_pipe_path, sizeof(notif_pipe) - 1);
    notif_pipe[sizeof(notif_pipe) - 1] = '\0';

    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);

    // create pipes
    if (mkfifo(req_pipe_path, 0666) != 0) {
        perror("[ERR]: mkfifo failed");
        return 1;
    }

    if (mkfifo(resp_pipe_path, 0666) != 0) {
        perror("[ERR]: mkfifo failed");
        return 1;
    }

    if (mkfifo(notif_pipe_path, 0666) != 0) {
        perror("[ERR]: mkfifo failed");
        return 1;
    }

    // create message to request connection
    char msg[122];
    memset(msg, '\0', sizeof(msg));
    msg[0] = OP_CODE_CONNECT;
    memcpy(msg + 1, req_pipe_path, 40);
    memcpy(msg + 41, resp_pipe_path, 40);
    memcpy(msg + 81, notif_pipe_path, 40);

    // open pipe server_pipe_path to write
    int server_pipe = open(server_pipe_path, O_WRONLY);

    if (server_pipe == -1) {
        perror("[ERR]: open failed");
        return 1;
    }

    if (write_all(server_pipe, msg, 122) != 1) {
        perror("[ERR]: write_all failed");
        return 1;
    }

    close(server_pipe);

    resp_pipe_fd = open(resp_pipe, O_RDONLY);

    if (resp_pipe_fd == -1) {
        perror("[ERR]: open failed");
        return 1;
    }
    // open pipes

    req_pipe_fd = open(req_pipe, O_WRONLY);

    if (req_pipe_fd == -1) {
        perror("[ERR]: open failed");
        return 1;
    }

    *notif_pipe_fd = open(notif_pipe, O_RDONLY);

    if (*notif_pipe_fd == -1) {
        perror("[ERR]: open failed");
        return 1;
    }

    // read response
    char response[3];
    if (read_all(resp_pipe_fd, response, 3, NULL) != 1) {
        // perror("[ERR]: read_all failed");
        return 1;
    }

    if (response[1] == '1') {
        write_all(STDOUT_FILENO, "Server returned 1 for operation: connect\n",
                  41);
        return 1;
    }

    write_all(STDOUT_FILENO, "Server returned 0 for operation: connect\n", 41);
    return 0;
}

int kvs_disconnect() {
    // create message to request disconnection
    char msg[2];
    msg[0] = OP_CODE_DISCONNECT;
    msg[1] = '\0';
    if (write_all(req_pipe_fd, msg, 2) != 1) {
        perror("[ERR]: write_all failed");
        return 1;
    }

    // read response
    char response[3];

    if (read_all(resp_pipe_fd, response, 3, NULL) != 1) {
        // perror("[ERR]: read_all failed");
        return 1;
    }

    if (response[1] == '1') {
        write_all(STDOUT_FILENO,
                  "Server returned 1 for operation: disconnect\n", 44);
        return 1;
    }

    write_all(STDOUT_FILENO, "Server returned 0 for operation: disconnect\n",
              44);

    close(req_pipe_fd);
    close(resp_pipe_fd);

    unlink(req_pipe);
    unlink(resp_pipe);
    unlink(notif_pipe);

    return 0;
}

int kvs_subscribe(const char *key) {
    // send subscribe message to request pipe and wait for response in
    // response pipe
    char msg[42];
    memset(msg, '\0', sizeof(msg));
    msg[0] = OP_CODE_SUBSCRIBE;

    // strncpy(msg + 1, key, 40);
    memcpy(msg + 1, key, 40);

    if (write_all(req_pipe_fd, msg, 42) != 1) {
        perror("[ERR]: write_all failed");
        return 1;
    }

    // read response
    char response[3];

    if (read_all(resp_pipe_fd, response, 3, NULL) != 1) {
        // perror("[ERR]: read_all failed");
        return 1;
    }

    if (response[1] == '1') {
        write_all(STDOUT_FILENO, "Server returned 1 for operation: subscribe\n",
                  43);
        return 1;
    }

    write_all(STDOUT_FILENO, "Server returned 0 for operation: subscribe\n",
              43);

    return 0;
}

int kvs_unsubscribe(const char *key) {
    // send unsubscribe message to request pipe and wait for response in
    // response pipe

    // create message to request unsubscription
    char msg[42];
    memset(msg, '\0', sizeof(msg));
    msg[0] = OP_CODE_UNSUBSCRIBE;

    strncpy(msg + 1, key, 40);

    if (write_all(req_pipe_fd, msg, 42) != 1) {
        perror("[ERR]: write_all failed");
        return 1;
    }

    // read response
    char response[3];

    if (read_all(resp_pipe_fd, response, 3, NULL) != 1) {
        // perror("[ERR]: read_all failed");
        return 1;
    }

    if (response[1] == '1') {
        write_all(STDOUT_FILENO,
                  "Server returned 1 for operation: unsubscribe\n", 45);
        return 1;
    }

    write_all(STDOUT_FILENO, "Server returned 0 for operation: unsubscribe\n",
              45);

    return 0;
}
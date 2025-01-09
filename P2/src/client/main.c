#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "src/client/api.h"
#include "src/common/constants.h"
#include "src/common/io.h"

// typedef struct {
//     char *notif_pipe;
//     int *running;
// } NotificationsThreadArgs;

void *notifications_thread(void *arg) {
    // NotificationsThreadArgs *args = (NotificationsThreadArgs *)arg;

    int notif_pipe = *(int *)arg;

    // char notif_pipe_path[256];
    // strcpy(notif_pipe_path, args->notif_pipe);

    // int notif_pipe = open(notif_pipe_path, O_RDONLY);
    // if (notif_pipe == -1) {
    //     fprintf(stderr, "Failed to open notifications pipe\n");
    //     return NULL;
    // }

    while (1) {
        // read notification
        char buffer[80] = {0};
        if (read_all(notif_pipe, buffer, 80, NULL) == -1) {
            fprintf(stderr, "Failed to read notification\n");
            break;
        }

        // parse notification
        char key[MAX_STRING_SIZE] = {0};
        char value[MAX_STRING_SIZE] = {0};

        for (int i = 0; i < 40; i++) {
            key[i] = buffer[i];
            value[i] = buffer[i + 40];
        }

        // write "(<key>, <value>)" using write_all
        char msg[240] = {0};
        snprintf(msg, 240, "(%s,%s)\n", key, value);
        if (write_all(STDOUT_FILENO, msg, 240) == -1) {
            fprintf(stderr, "Failed to write notification\n");
            break;
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <client_unique_id> <register_pipe_path>\n",
                argv[0]);
        return 1;
    }

    char req_pipe_path[256] = "/tmp/req";
    char resp_pipe_path[256] = "/tmp/resp";
    char notif_pipe_path[256] = "/tmp/notif";

    char keys[MAX_NUMBER_SUB][MAX_STRING_SIZE] = {0};
    unsigned int delay_ms;
    size_t num;

    strncat(req_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
    strncat(resp_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
    strncat(notif_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));

    int notif_pipe = -1;
    // TODO open pipes
    if (kvs_connect(req_pipe_path, resp_pipe_path, argv[2], notif_pipe_path,
                    &notif_pipe) != 0) {
        fprintf(stderr, "Failed to connect to the server\n");
        unlink(req_pipe_path);
        unlink(resp_pipe_path);
        unlink(notif_pipe_path);
        return 1;
    }

    // int running = 1;
    // NotificationsThreadArgs args = {
    //     .notif_pipe = notif_pipe_path,
    //     .running = &running,
    // };

    pthread_t NotificationsThread;

    if (pthread_create(&NotificationsThread, NULL, notifications_thread,
                       &notif_pipe) != 0) {
        fprintf(stderr, "Failed to create notifications thread\n");
        return 1;
    }

    while (1) {
        switch (get_next(STDIN_FILENO)) {
            case CMD_DISCONNECT:
                close(notif_pipe);

                if (kvs_disconnect() != 0) {
                    fprintf(stderr, "Failed to disconnect to the server\n");
                    return 1;
                }
                // end notifications thread
                // running = 0;
                pthread_join(NotificationsThread, NULL);

                printf("Disconnected from server\n");
                return 0;

            case CMD_SUBSCRIBE:
                num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
                if (num == 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                if (kvs_subscribe(keys[0])) {
                    fprintf(stderr, "Command subscribe failed\n");
                }

                break;

            case CMD_UNSUBSCRIBE:
                num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
                if (num == 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                if (kvs_unsubscribe(keys[0])) {
                    fprintf(stderr, "Command subscribe failed\n");
                }

                break;

            case CMD_DELAY:
                if (parse_delay(STDIN_FILENO, &delay_ms) == -1) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                if (delay_ms > 0) {
                    printf("Waiting...\n");
                    delay(delay_ms);
                }
                break;

            case CMD_INVALID:
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                break;

            case CMD_EMPTY:
                break;

            case EOC:
                // input should end in a disconnect, or it will loop here
                // forever
                break;
        }
    }
}

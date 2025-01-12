#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../common/io.h"
#include "../common/protocol.h"
#include "constants.h"
#include "operations.h"
#include "parser.h"
#include "subscriptions.h"
#include "utils.h"

// variables for backup
int active_backups = 0;
int max_backups;
pthread_mutex_t backup_mutex = PTHREAD_MUTEX_INITIALIZER;

// variables for buffer host-managers
ClientPipes buffer[MAX_SESSION_COUNT];
int in = 0;
int out = 0;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t empty, full;

void initialize_buffer() {
    sem_init(&empty, 0, MAX_SESSION_COUNT);  // Buffer está vazio
    sem_init(&full, 0, 0);                   // Nenhum item está disponível
}

// Insere um item no buffer
void insert_buffer(ClientPipes item) {
    sem_wait(&empty);                   // Espera espaço disponível
    pthread_mutex_lock(&buffer_mutex);  // Entra na seção crítica

    buffer[in] = item;                  // Adiciona o item no buffer
    in = (in + 1) % MAX_SESSION_COUNT;  // Incrementa o índice circular

    pthread_mutex_unlock(&buffer_mutex);  // Sai da seção crítica
    sem_post(&full);  // Incrementa o contador de itens disponíveis
}

// Remove um item do buffer
ClientPipes remove_buffer() {
    sem_wait(&full);                    // Espera por itens disponíveis
    pthread_mutex_lock(&buffer_mutex);  // Entra na seção crítica

    ClientPipes item = buffer[out];       // Remove o item do buffer
    out = (out + 1) % MAX_SESSION_COUNT;  // Incrementa o índice circular

    pthread_mutex_unlock(&buffer_mutex);  // Sai da seção crítica
    sem_post(&empty);  // Incrementa o contador de espaços disponíveis

    return item;
}

// Libera os recursos do buffer
void cleanup_buffer() {
    sem_destroy(&empty);
    sem_destroy(&full);
    pthread_mutex_destroy(&buffer_mutex);
}

// manager thread
void *managerThread() {
    while (1) {
        ClientPipes client_pipes = remove_buffer();
        printf("removed\n");

        printf("manager\n");
        printf("req_pipe: %s\n", client_pipes.req_pipe);
        printf("res_pipe: %s\n", client_pipes.res_pipe);
        printf("notif_pipe: %s\n", client_pipes.notif_pipe);

        int res_pipe_fd = open(client_pipes.res_pipe, O_WRONLY);
        printf("pipe res opened\n");

        int req_pipe_fd = open(client_pipes.req_pipe, O_RDONLY);
        printf("pipe req opened\n");

        int notif_pipe_fd = open(client_pipes.notif_pipe, O_WRONLY);
        printf("pipe notif opened\n");

        // if res_pipe_fd fails to open, we cant send response to client
        if (res_pipe_fd == -1) {
            printf("aqui\n");
            fprintf(stderr, "Failed to open pipe\n");
            return NULL;
        }

        // if req_pipe_fd or notif_pipe_fd fails to open, we communicate with
        // the client that the connection failed
        if (req_pipe_fd == -1 || notif_pipe_fd == -1) {
            fprintf(stderr, "Failed to open pipe\n");
            write_all(res_pipe_fd, "11", 2);
            close(res_pipe_fd);
            return NULL;
        }

        // send response to client
        char response[3] = {OP_CODE_CONNECT, 0, '\0'};
        printf("%s", response);

        if (write_all(res_pipe_fd, response, 3) != 1) {
            perror("[ERR]: write_all failed");
            return NULL;
        }

        printf("response sent\n");

        int flag = 1;

        while (flag == 1) {
            char opcode;

            // if (read_string(req_pipe_fd, request) == -1) {
            //     perror("[ERR]: read_all failed");
            //     return NULL;
            // }

            if (read_all(req_pipe_fd, &opcode, 1, NULL) == -1) {
                perror("[ERR]: read_all failed");
                return NULL;
            }

            switch (opcode) {
                case OP_CODE_SUBSCRIBE:
                    printf("subscribe\n");
                    char key[41] = {0};

                    if (read_all(req_pipe_fd, key, 40, NULL) == -1) {
                        perror("[ERR]: read_all failed");
                        return NULL;
                    }
                    // printf("%s\n", key);

                    // memcpy(key, request + 1, 41);

                    if (key_exists(key) == 0) {
                        printf("key not exists\n");

                        char response_subscribe[3] = {OP_CODE_SUBSCRIBE, '0',
                                                      '\0'};

                        if (write_all(res_pipe_fd, response_subscribe, 3) !=
                            1) {
                            perror("[ERR]: write_all failed");
                            return NULL;
                        }
                        break;
                    }

                    add_subscription(key, notif_pipe_fd);

                    printf("subscribed\n");

                    char response_subscribe[3] = {OP_CODE_SUBSCRIBE, '1', '\0'};

                    if (write_all(res_pipe_fd, response_subscribe, 3) != 1) {
                        perror("[ERR]: write_all failed");
                        return NULL;
                    }

                    break;

                case OP_CODE_UNSUBSCRIBE:
                    printf("unsubscribe\n");
                    char key_unsub[41] = {0};

                    if (read_all(req_pipe_fd, key_unsub, 40, NULL) == -1) {
                        perror("[ERR]: read_all failed");
                        return NULL;
                    }

                    if (is_suscribed(key_unsub, notif_pipe_fd) == 0) {
                        char response_unsubscribe[3] = {OP_CODE_UNSUBSCRIBE,
                                                        '1', '\0'};

                        if (write_all(res_pipe_fd, response_unsubscribe, 3) !=
                            1) {
                            perror("[ERR]: write_all failed");
                            return NULL;
                        }
                        break;
                    }

                    remove_subscription(key_unsub, notif_pipe_fd);

                    char response_unsubscribe[3] = {OP_CODE_UNSUBSCRIBE, '0',
                                                    '\0'};

                    if (write_all(res_pipe_fd, response_unsubscribe, 3) != 1) {
                        perror("[ERR]: write_all failed");
                        return NULL;
                    }

                    break;

                case OP_CODE_DISCONNECT:
                    printf("disconnect\n");

                    if (close(req_pipe_fd) == -1 ||
                        close(notif_pipe_fd) == -1) {
                        perror("[ERR]: close failed");
                        write_all(res_pipe_fd, "21", 2);
                        close(res_pipe_fd);
                        flag = 0;
                        break;
                    }

                    remove_all_subscriptions(notif_pipe_fd);

                    char response_disconnect[3] = {OP_CODE_DISCONNECT, '0',
                                                   '\0'};

                    write_all(res_pipe_fd, response_disconnect, 3);

                    close(res_pipe_fd);
                    flag = 0;
                    break;

                default:
                    break;
            }
        }
    }
}

// host thread
void *hostThread(void *arg) {
    char *pipe_path = (char *)arg;
    int pipe_fd = open(pipe_path, O_RDWR);
    // int pipe_fd = open(pipe_path, O_RDONLY);

    if (pipe_fd == -1) {
        fprintf(stderr, "Failed to open pipe\n");
        return NULL;
    }

    while (1) {
        char op_code;
        char req_pipe[41];
        char res_pipe[41];
        char notif_pipe[41];

        if (read_all(pipe_fd, &op_code, 1, NULL) != 1) {
            perror("[ERR]: read_all failed");
            return NULL;
        }

        if (op_code == OP_CODE_CONNECT) {
            printf("opasnf oipamnd fiopo\n");

            if (read_all(pipe_fd, req_pipe, 40, NULL) != 1) {
                perror("[ERR]: read_all failed");
                return NULL;
            }

            printf("ola\n");
            req_pipe[40] = '\0';

            if (read_all(pipe_fd, res_pipe, 40, NULL) != 1) {
                perror("[ERR]: read_all failed");
                return NULL;
            }
            printf("ola1\n");
            res_pipe[40] = '\0';

            if (read_all(pipe_fd, notif_pipe, 40, NULL) != 1) {
                perror("[ERR]: read_all failed");
                return NULL;
            }

            printf("ola2\n");

            notif_pipe[40] = '\0';

            printf("anfitria\n");
            printf("req_pipe: %s\n", req_pipe);
            printf("res_pipe: %s\n", res_pipe);
            printf("notif_pipe: %s\n", notif_pipe);

            ClientPipes client_pipes;
            memcpy(client_pipes.req_pipe, req_pipe, 41);
            memcpy(client_pipes.res_pipe, res_pipe, 41);
            memcpy(client_pipes.notif_pipe, notif_pipe, 41);

            insert_buffer(client_pipes);
            printf("inserted\n");
        }
    }

    // ficar a ler da pipe
}

void kvs_main(char *job_name) {
    // flag used to control the loop
    int flag = 1;
    int num_backup_name = 0;

    int file_in = open(job_name, O_RDONLY);

    if (file_in == -1) {
        fprintf(stderr, "Failed to open file\n");
        return;
    }

    // create new path for output file
    char *job_out_path = job_name;
    char *ponto = strrchr(job_out_path, '.');
    strcpy(ponto, ".out");

    int file_out = open(job_out_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (file_out == -1) {
        fprintf(stderr, "Failed to open file\n");
        free(job_out_path);
        return;
    }

    while (flag) {
        char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
        char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
        unsigned int delay;
        size_t num_pairs;

        switch (get_next(file_in)) {
            case CMD_WRITE:
                num_pairs = parse_write(file_in, keys, values, MAX_WRITE_SIZE,
                                        MAX_STRING_SIZE);
                if (num_pairs == 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                // sort the pairs by alphabetical order of keys
                sortPairs(num_pairs, keys, values);

                if (kvs_write(num_pairs, keys, values)) {
                    fprintf(stderr, "Failed to write pair\n");
                }

                break;

            case CMD_READ:
                num_pairs = parse_read_delete(file_in, keys, MAX_WRITE_SIZE,
                                              MAX_STRING_SIZE);

                if (num_pairs == 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                // sort the pairs by alphabetical order of keys
                sortPairs(num_pairs, keys, values);

                if (kvs_read(num_pairs, keys, file_out)) {
                    fprintf(stderr, "Failed to read pair\n");
                }
                break;

            case CMD_DELETE:
                num_pairs = parse_read_delete(file_in, keys, MAX_WRITE_SIZE,
                                              MAX_STRING_SIZE);

                if (num_pairs == 0) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                // sort the pairs by alphabetical order of keys
                sortPairs(num_pairs, keys, values);

                if (kvs_delete(num_pairs, keys, file_out)) {
                    fprintf(stderr, "Failed to delete pair\n");
                }
                break;

            case CMD_SHOW:
                kvs_show(file_out);
                break;

            case CMD_WAIT:
                if (parse_wait(file_in, &delay, NULL) == -1) {
                    fprintf(stderr, "Invalid command. See HELP for usage\n");
                    continue;
                }

                if (delay > 0) {
                    tryWrite(file_out, "Waiting...\n", 11);
                    kvs_wait(delay);
                }
                break;

            case CMD_BACKUP:
                num_backup_name++;

                while (1) {
                    mutex_lock(&backup_mutex);
                    if (active_backups < max_backups) {
                        active_backups++;

                        mutex_unlock(&backup_mutex);
                        break;
                    }
                    mutex_unlock(&backup_mutex);
                }

                if (kvs_backup(job_name, num_backup_name)) {
                    fprintf(stderr, "Failed to perform backup.\n");
                    num_backup_name--;
                }

                mutex_lock(&backup_mutex);
                active_backups--;
                mutex_unlock(&backup_mutex);

                break;

            case CMD_INVALID:
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                break;

            case CMD_HELP:
                printf(
                    "Available commands:\n"
                    "  WRITE [(key,value)(key2,value2),...]\n"
                    "  READ [key,key2,...]\n"
                    "  DELETE [key,key2,...]\n"
                    "  SHOW\n"
                    "  WAIT <delay_ms>\n"
                    "  BACKUP\n"
                    "  HELP\n");

                break;

            case CMD_EMPTY:
                break;

            case EOC:
                flag = 0;
                break;
        }
    }
    close(file_in);
    close(file_out);

    return;
}

void *thread_function(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int index;

    while (1) {
        mutex_lock(&data->mutex);

        if (data->current_file >= data->num_files) {
            mutex_unlock(&data->mutex);
            break;
        }

        index = data->current_file;
        data->current_file++;

        mutex_unlock(&data->mutex);

        kvs_main(data->file_paths[index]);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr,
                "Usage: %s <directory_path> <number_threads> "
                "<number_backups> <register_pipe_path>\n",
                argv[0]);
        return 1;
    }

    char *directoryPath = argv[1];
    DIR *dir = opendir(directoryPath);
    max_backups = atoi(argv[3]);
    int num_threads = atoi(argv[2]);
    char *pipe_path = argv[4];

    if (dir == NULL) {
        fprintf(stderr, "Failed to open directory\n");
        return 1;
    }

    if (max_backups < 0) {
        fprintf(stderr, "Invalid number of backups\n");
        closedir(dir);
        return 1;
    }

    if (num_threads < 0) {
        fprintf(stderr, "Invalid number of threads\n");
        closedir(dir);
        return 1;
    }

    unlink(pipe_path);

    if (mkfifo(pipe_path, 0666) != 0) {
        fprintf(stderr, "Failed to create pipe\n");
        closedir(dir);
        return 1;
    }

    if (kvs_init()) {
        fprintf(stderr, "Failed to initialize KVS\n");
        closedir(dir);
        return 1;
    }

    init_subscriptions();

    initialize_buffer();

    pthread_t host_thread;
    pthread_create(&host_thread, NULL, hostThread, pipe_path);

    pthread_t manager_threads[MAX_SESSION_COUNT];
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        pthread_create(&manager_threads[i], NULL, managerThread, NULL);
    }

    int job_count = 0;
    char **jobs = getJobs(&job_count, dir, directoryPath);

    ThreadData data = {
        .file_paths = jobs,
        .num_files = job_count,
        .current_file = 0,
    };

    mutex_init(&data.mutex);

    pthread_t threads[num_threads];

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, thread_function, &data);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_join(host_thread, NULL);

    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        pthread_join(manager_threads[i], NULL);
    }

    kvs_terminate();

    for (int i = 0; i < job_count; i++) {
        free(jobs[i]);
    }
    free(jobs);

    mutex_destroy(&data.mutex);
    mutex_destroy(&backup_mutex);

    return 0;
}

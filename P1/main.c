#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"
#include "utils.h"

int active_backups = 0;
int max_backups;
pthread_mutex_t backup_mutex = PTHREAD_MUTEX_INITIALIZER;

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
    if (argc < 4) {
        fprintf(stderr,
                "Usage: %s <directory_path> <number_backups> "
                "<number_threads>\n",
                argv[0]);
        return 1;
    }

    char *directoryPath = argv[1];
    DIR *dir = opendir(directoryPath);
    max_backups = atoi(argv[2]);
    int num_threads = atoi(argv[3]);

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

    if (kvs_init()) {
        fprintf(stderr, "Failed to initialize KVS\n");
        closedir(dir);
        return 1;
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

    kvs_terminate();

    for (int i = 0; i < job_count; i++) {
        free(jobs[i]);
    }
    free(jobs);

    mutex_destroy(&data.mutex);
    mutex_destroy(&backup_mutex);

    return 0;
}

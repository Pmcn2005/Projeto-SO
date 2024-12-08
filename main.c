#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"
#include "utils.h"

const char *directoryPath;

int num_max_backups;
int current_backups = 0;

void kvs_main(int file_in, int file_out, const char *job_name) {
    int flag = 1;
    int exectuded_backups = 0;

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

                // ordenar keys e values por ordem alfabética de keys
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

                // ordenar keys e values por ordem alfabética de keys
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

                // ordenar keys e values por ordem alfabética de keys
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
                    printf("Waiting...\n");
                    kvs_wait(delay);
                }
                break;

            case CMD_BACKUP:
                exectuded_backups++;

                while (current_backups >= num_max_backups) {
                    wait(NULL);  // Espera por um processo filho terminar
                    current_backups--;
                }

                if (kvs_backup(job_name, exectuded_backups)) {
                    fprintf(stderr, "Failed to perform backup.\n");
                    exectuded_backups--;
                }
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
                    "  BACKUP\n"  // Not implemented
                    "  HELP\n");

                break;

            case CMD_EMPTY:
                break;

            case EOC:
                flag = 0;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <directory_path> <number_backups>\n",
                argv[0]);
        return 1;
    }

    directoryPath = argv[1];
    DIR *dir = opendir(directoryPath);
    num_max_backups = atoi(argv[2]);

    if (dir == NULL) {
        fprintf(stderr, "Failed to open directory\n");
        return 1;
    }

    if (num_max_backups < 0) {
        fprintf(stderr, "Invalid number of backups\n");
        return 1;
    }

    if (kvs_init()) {
        fprintf(stderr, "Failed to initialize KVS\n");
        return 1;
    }

    char **jobs = NULL;
    int job_count = 0;
    getJobs(&jobs, &job_count, dir, directoryPath);

    for (ssize_t i = 0; i < job_count; i++) {
        int job_file = open(jobs[i], O_RDONLY);
        if (job_file == -1) {
            fprintf(stderr, "Failed to open file\n");
            return 1;
        }

        // create new path for output file
        char *job_out_path = strdup(jobs[i]);
        char *ponto = strrchr(job_out_path, '.');
        strcpy(ponto, ".out");

        int job_out = open(job_out_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

        kvs_main(job_file, job_out, jobs[i]);

        close(job_file);
        close(job_out);
        free(job_out_path);
    }

    kvs_terminate();
    free(dir);
    for (int i = 0; i < job_count; i++) {
        free(jobs[i]);
    }
    free(jobs);

    return 0;
}

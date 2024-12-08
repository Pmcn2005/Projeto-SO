#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "constants.h"
#include "kvs.h"
#include "utils.h"

static struct HashTable* kvs_table = NULL;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
    return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

int kvs_init() {
    if (kvs_table != NULL) {
        fprintf(stderr, "KVS state has already been initialized\n");
        return 1;
    }

    kvs_table = create_hash_table();
    return kvs_table == NULL;
}

int kvs_terminate() {
    if (kvs_table == NULL) {
        fprintf(stderr, "KVS state must be initialized\n");
        return 1;
    }

    free_table(kvs_table);
    return 0;
}

int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE],
              char values[][MAX_STRING_SIZE]) {
    if (kvs_table == NULL) {
        fprintf(stderr, "KVS state must be initialized\n");
        return 1;
    }

    for (size_t i = 0; i < num_pairs; i++) {
        if (write_pair(kvs_table, keys[i], values[i]) != 0) {
            fprintf(stderr, "Failed to write keypair (%s,%s)\n", keys[i],
                    values[i]);
        }
    }

    return 0;
}

int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd_out) {
    if (kvs_table == NULL) {
        fprintf(stderr, "KVS state must be initialized\n");
        return 1;
    }

    tryWrite(fd_out, "[", 1);
    for (size_t i = 0; i < num_pairs; i++) {
        char* result = read_pair(kvs_table, keys[i]);
        if (result == NULL) {
            char buffer[MAX_STRING_SIZE * 2 + 12];
            sprintf(buffer, "(%s,KVSERROR)", keys[i]);
            tryWrite(fd_out, buffer, strlen(buffer));

        } else {
            char buffer[MAX_STRING_SIZE * 2 + 12];  // Adjust size as needed
            sprintf(buffer, "(%s,%s)", keys[i], result);
            tryWrite(fd_out, buffer, strlen(buffer));
        }
        free(result);
    }
    tryWrite(fd_out, "]\n", 2);
    return 0;
}

int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd_out) {
    if (kvs_table == NULL) {
        fprintf(stderr, "KVS state must be initialized\n");
        return 1;
    }
    int aux = 0;

    for (size_t i = 0; i < num_pairs; i++) {
        if (delete_pair(kvs_table, keys[i]) != 0) {
            if (!aux) {
                tryWrite(fd_out, "[", 1);
                aux = 1;
            }
            char buffer[MAX_STRING_SIZE * 2 + 12];
            sprintf(buffer, "(%s,KVSMISSING)", keys[i]);
            tryWrite(fd_out, buffer, strlen(buffer));
        }
    }
    if (aux) {
        tryWrite(fd_out, "]\n", 2);
    }

    return 0;
}

void kvs_show(int fd_out) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        KeyNode* keyNode = kvs_table->table[i];
        while (keyNode != NULL) {
            // printf("(%s, %s)\n", keyNode->key, keyNode->value);
            char buffer[MAX_STRING_SIZE * 2 + 12];  // Adjust size as needed
            sprintf(buffer, "(%s, %s)\n", keyNode->key, keyNode->value);
            tryWrite(fd_out, buffer, strlen(buffer));

            keyNode = keyNode->next;  // Move to the next node
        }
    }
}

int kvs_backup(const char* job_name, int current_backup) {
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Failed to create backup process\n");
        return 1;
    } else if (pid == 0) {
        // Child process

        // create new path for backup file
        char* backup_path = strdup(job_name);
        backup_path[strlen(backup_path) - 4] = '\0';

        char buffer[MAX_JOB_FILE_NAME_SIZE];
        sprintf(buffer, "-%d.bck", current_backup);
        backup_path =
            realloc(backup_path, strlen(backup_path) + strlen(buffer) + 1);
        strcat(backup_path, buffer);

        int backup_file = open(backup_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (backup_file == -1) {
            fprintf(stderr, "Failed to open backup file\n");
            return 1;
        }

        kvs_show(backup_file);

        close(backup_file);
        free(backup_path);

        exit(0);

    } else {
        // Parent process
        return 0;
    }

    return 0;
}

void kvs_wait(unsigned int delay_ms) {
    struct timespec delay = delay_to_timespec(delay_ms);
    nanosleep(&delay, NULL);
}
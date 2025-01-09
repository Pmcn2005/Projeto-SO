#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "constants.h"

// function that will read the files from a directory and add them to the list
// if they are .job files
char **getJobs(int *job_count, DIR *dir, char *directory_path) {
    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".job") != NULL) {
            count++;
        }
    }

    char **jobs = malloc((size_t)count * sizeof(char *));
    count = 0;

    rewinddir(dir);

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".job") != NULL) {
            // verify if last caracter / is present in directory_path
            if (directory_path[strlen(directory_path) - 1] != '/') {
                size_t path_len =
                    strlen(directory_path) + strlen(entry->d_name) + 2;
                jobs[count] = malloc(path_len * sizeof(char));
                snprintf(jobs[count], path_len, "%s/%s", directory_path,
                         entry->d_name);
            } else {
                size_t path_len =
                    strlen(directory_path) + strlen(entry->d_name) + 1;
                jobs[count] = malloc(path_len * sizeof(char));
                snprintf(jobs[count], path_len, "%s%s", directory_path,
                         entry->d_name);
            }
            count++;
        }
    }

    *job_count = count;
    closedir(dir);
    free(entry);
    return jobs;
}

// function to order the keys and values in alphabetical order of keys
void sortPairs(size_t num_pairs, char keys[][MAX_STRING_SIZE],
               char values[][MAX_STRING_SIZE]) {
    for (size_t i = 0; i < num_pairs; i++) {
        for (size_t j = i + 1; j < num_pairs; j++) {
            if (strcmp(keys[i], keys[j]) > 0) {
                char temp[MAX_STRING_SIZE];
                strcpy(temp, keys[i]);
                strcpy(keys[i], keys[j]);
                strcpy(keys[j], temp);
                strcpy(temp, values[i]);
                strcpy(values[i], values[j]);
                strcpy(values[j], temp);
            }
        }
    }
}

void tryWrite(int fd, const char *buffer, size_t size) {
    if (write(fd, buffer, size) != (ssize_t)size) {
        fprintf(stderr, "Failed to write to file\n");
        exit(1);
    }
}

void rwl_wrlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_wrlock(rwl) != 0) {
        perror("Failed to lock RWlock");
        exit(EXIT_FAILURE);
    }
}

void rwl_rdlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_rdlock(rwl) != 0) {
        perror("Failed to lock RWlock");
        exit(EXIT_FAILURE);
    }
}

void rwl_unlock(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_unlock(rwl) != 0) {
        perror("Failed to unlock RWlock");
        exit(EXIT_FAILURE);
    }
}

void rwl_init(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_init(rwl, NULL) != 0) {
        perror("Failed to init RWlock");
        exit(EXIT_FAILURE);
    }
}

void rwl_destroy(pthread_rwlock_t *rwl) {
    if (pthread_rwlock_destroy(rwl) != 0) {
        perror("Failed to destroy RWlock");
        exit(EXIT_FAILURE);
    }
}

void mutex_lock(pthread_mutex_t *mutex) {
    if (pthread_mutex_lock(mutex) != 0) {
        perror("Failed to lock Mutex");
        exit(EXIT_FAILURE);
    }
}

void mutex_unlock(pthread_mutex_t *mutex) {
    if (pthread_mutex_unlock(mutex) != 0) {
        perror("Failed to unlock Mutex");
        exit(EXIT_FAILURE);
    }
}

void mutex_init(pthread_mutex_t *mutex) {
    if (pthread_mutex_init(mutex, NULL) != 0) {
        perror("Failed to init Mutex");
        exit(EXIT_FAILURE);
    }
}

void mutex_destroy(pthread_mutex_t *mutex) {
    if (pthread_mutex_destroy(mutex) != 0) {
        perror("Failed to destroy Mutex");
        exit(EXIT_FAILURE);
    }
}
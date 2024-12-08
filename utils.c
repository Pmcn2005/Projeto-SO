#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_JOB_FILE_NAME_SIZE 256

#include "constants.h"

// this will read the files from a directory and add them to the list if they
// are .job files
void getJobs(char ***jobs, int *job_count, DIR *dir,
             const char *directory_path) {
    struct dirent *entry;
    int count = 0;
    int capacity = 10;  // Initial capacity
    *jobs = malloc((size_t)capacity * sizeof(char *));

    while ((entry = readdir(dir)) != NULL) {
        if (count >= capacity) {
            capacity *= 2;
            *jobs = realloc(*jobs, (size_t)capacity * sizeof(char *));
        }
        if (strstr(entry->d_name, ".job") != NULL) {
            size_t path_len =
                strlen(directory_path) + strlen(entry->d_name) + 2;
            (*jobs)[count] = malloc(path_len);
            snprintf((*jobs)[count], path_len, "%s/%s", directory_path,
                     entry->d_name);
            count++;
        }
    }
    *job_count = count;
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

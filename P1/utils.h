#ifndef UTILS_H
#define UTILS_H

#include <dirent.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>

#include "constants.h"

/// Struct to hold the data for the threads.
typedef struct {
    char **file_paths;
    int num_files;
    int current_file;
    pthread_mutex_t mutex;
} ThreadData;

/// Returns a list of all .job files in the given directory.
/// @param job_count Pointer to the number of jobs found.
/// @param dir Directory to be read.
/// @param directory_path Path to the directory.
/// @return List of all .job files in the given directory.
char **getJobs(int *job_count, DIR *dir, char *directory_path);

/// Orders the keys and values in alphabetical order of keys
/// @param num_pairs Number of pairs to be sorted.
/// @param keys Array of keys to be sorted.
/// @param values Array of values to be sorted.
void sortPairs(size_t num_pairs, char keys[][MAX_STRING_SIZE],
               char values[][MAX_STRING_SIZE]);

/// Writes to a file descriptor.
/// @param fd File descriptor to write to.
/// @param buffer Buffer to be written.
/// @param size Size of the buffer.
void tryWrite(int fd, const char *buffer, size_t size);

/// Locks the rwlock to write-read.
/// Exits with failure if unsuccessful.
void rwl_wrlock(pthread_rwlock_t *rwl);

/// Locks the rwlock to read-only.
/// Exits with failure if unsuccessful.
void rwl_rdlock(pthread_rwlock_t *rwl);

/// Unlocks the rwlock.
/// Exits with failure if unsuccessful.
void rwl_unlock(pthread_rwlock_t *rwl);

/// Initializes the rwlock.
/// Exits with failure if unsuccessful.
void rwl_init(pthread_rwlock_t *rwl);

/// Destroys the rwlock.
/// Exits with failure if unsuccessful.
void rwl_destroy(pthread_rwlock_t *rwl);

/// Locks the mutex.
/// Exits with failure if unsuccessful.
void mutex_lock(pthread_mutex_t *mutex);

/// Unlocks the mutex.
/// Exits with failure if unsuccessful.
void mutex_unlock(pthread_mutex_t *mutex);

/// Initializes the mutex.
/// Exits with failure if unsuccessful.
void mutex_init(pthread_mutex_t *mutex);

/// Destroys the mutex.
/// Exits with failure if unsuccessful.
void mutex_destroy(pthread_mutex_t *mutex);
#endif  // UTILS_H
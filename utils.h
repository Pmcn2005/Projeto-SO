#ifndef UTILS_H
#define UTILS_H

#include <dirent.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>

#include "constants.h"

char **getJobs(int *job_count, DIR *dir, char *directory_path);

void sortPairs(size_t num_pairs, char keys[][MAX_STRING_SIZE],
               char values[][MAX_STRING_SIZE]);

void tryWrite(int fd, const char *buffer, size_t size);

void rwl_wrlock(pthread_rwlock_t *rwl);

void rwl_rdlock(pthread_rwlock_t *rwl);

void rwl_unlock(pthread_rwlock_t *rwl);

void rwl_init(pthread_rwlock_t *rwl);

void rwl_destroy(pthread_rwlock_t *rwl);

void mutex_lock(pthread_mutex_t *mutex);

void mutex_unlock(pthread_mutex_t *mutex);

void mutex_init(pthread_mutex_t *mutex);

void mutex_destroy(pthread_mutex_t *mutex);
#endif  // UTILS_H
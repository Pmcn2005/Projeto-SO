#include "subscriptions.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/io.h"

static Subscription *subscriptions[HASH_SIZE];

// Função de hash simples para mapear uma chave a um índice
int hash_function(const char *key) {
    int hash = 0;
    while (*key) {
        hash = (hash + *key) % HASH_SIZE;
        key++;
    }
    return hash;
}

void init_subscriptions() {
    for (int i = 0; i < HASH_SIZE; i++) {
        subscriptions[i] = malloc(sizeof(Subscription));
        subscriptions[i]->fifo_list = NULL;
        pthread_mutex_init(&subscriptions[i]->mutex, NULL);
    }
}

void add_subscription(const char *key, const int fifo_fd) {
    int index = hash_function(key);

    pthread_mutex_lock(&subscriptions[index]->mutex);

    Subscription *sub = subscriptions[index];
    while (sub && strcmp(sub->key, key) != 0) {
        sub = (Subscription *)sub->fifo_list;  // Avançar na lista de colisões
    }

    if (!sub) {
        // Se a subscrição ainda não existir, cria uma nova
        sub = malloc(sizeof(Subscription));
        strcpy(sub->key, key);
        sub->fifo_list = NULL;
        pthread_mutex_init(&sub->mutex, NULL);

        // Adicionar ao início da lista de colisões
        sub->fifo_list = (FifoNode *)subscriptions[index];
        subscriptions[index] = sub;
    }

    // Adicionar o FIFO à lista de subscritores
    FifoNode *node = malloc(sizeof(FifoNode));
    node->fd = fifo_fd;
    node->next = sub->fifo_list;
    sub->fifo_list = node;

    pthread_mutex_unlock(&subscriptions[index]->mutex);
}

void remove_subscription(const char *key, const int fifo_fd) {
    int index = hash_function(key);

    pthread_mutex_lock(&subscriptions[index]->mutex);

    Subscription *sub = subscriptions[index];
    while (sub && strcmp(sub->key, key) != 0) {
        sub = (Subscription *)sub->fifo_list;  // Avançar na lista de colisões
    }

    if (sub) {
        FifoNode **indirect = &sub->fifo_list;
        while (*indirect) {
            FifoNode *node = *indirect;
            if (node->fd == fifo_fd) {
                *indirect = node->next;
                free(node);
                break;
            }
            indirect = &node->next;
        }
    }

    pthread_mutex_unlock(&subscriptions[index]->mutex);
}

void notify_subscribers(const char *key, const char *new_value) {
    int index = hash_function(key);

    pthread_mutex_lock(&subscriptions[index]->mutex);

    Subscription *sub = subscriptions[index];
    while (sub && strcmp(sub->key, key) != 0) {
        sub = (Subscription *)sub->fifo_list;  // Avançar na lista de colisões
    }

    if (sub) {
        FifoNode *node = sub->fifo_list;
        while (node) {
            char message[82];  // 40 para chave + 40 para valor + '\0'
            char key_msg[41];
            char key_value[41];
            memset(key_msg, '\0', sizeof(key_msg));
            memset(key_value, '\0', sizeof(key_value));
            strncpy(key_msg, key, 40);
            strncpy(key_value, new_value, 40);

            snprintf(message, sizeof(message), "%s%s", key_msg, key_value);

            if (write_all(node->fd, message, strlen(message) + 1) != 1) {
                perror("[ERR]: write_all failed");
            }

            node = node->next;
        }
    }

    pthread_mutex_unlock(&subscriptions[index]->mutex);
}

void remove_all_subscriptions(const int fifo_fd) {
    for (int i = 0; i < HASH_SIZE; i++) {
        pthread_mutex_lock(&subscriptions[i]->mutex);

        Subscription *sub = subscriptions[i];
        while (sub) {
            FifoNode **indirect = &sub->fifo_list;
            while (*indirect) {
                FifoNode *node = *indirect;
                if (node->fd == fifo_fd) {
                    *indirect = node->next;
                    free(node);
                } else {
                    indirect = &node->next;
                }
            }
            sub =
                (Subscription *)sub->fifo_list;  // Avançar na lista de colisões
        }

        pthread_mutex_unlock(&subscriptions[i]->mutex);
    }
}

int is_suscribed(const char *key, const int fifo_fd) {
    int index = hash_function(key);

    pthread_mutex_lock(&subscriptions[index]->mutex);

    Subscription *sub = subscriptions[index];
    while (sub && strcmp(sub->key, key) != 0) {
        sub = (Subscription *)sub->fifo_list;  // Avançar na lista de colisões
    }

    if (sub) {
        FifoNode *node = sub->fifo_list;
        while (node) {
            if (node->fd == fifo_fd) {
                pthread_mutex_unlock(&subscriptions[index]->mutex);
                return 1;
            }
            node = node->next;
        }
    }

    pthread_mutex_unlock(&subscriptions[index]->mutex);
    return 0;
}

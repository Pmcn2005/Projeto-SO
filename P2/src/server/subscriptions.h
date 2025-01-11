#ifndef SUBSCRIPTIONS_H
#define SUBSCRIPTIONS_H

#define HASH_SIZE 128

#include <pthread.h>
#include <stddef.h>
// define a structure to store all the subscriptions to kvs from all the
// clients, a key can have multiple subscribers
// Estrutura para um nó na lista ligada de FIFOs
typedef struct FifoNode {
    // char fifo_path[40];     // Caminho do FIFO do cliente
    int fd;                 // File descriptor do FIFO do cliente
    struct FifoNode *next;  // Próximo nó na lista
} FifoNode;

// Estrutura para armazenar subscrições por chave
typedef struct Subscription {
    char key[41];           // A chave subscrita
    FifoNode *fifo_list;    // Lista ligada de FIFOs de clientes
    pthread_mutex_t mutex;  // Mutex para proteger acessos concorrentes
} Subscription;

// Função de hash simples para mapear uma chave a um índice
int hash_function(const char *key);

// Inicializa a tabela de subscrições
void init_subscriptions();

// Adiciona uma subscrição à tabela
void add_subscription(const char *key, const int fifo_fd);

// Remove uma subscrição da tabela
void remove_subscription(const char *key, const int fifo_fd);

// Notifica todos os subscritores de uma chave
void notify_subscribers(const char *key, const char *new_value);

// Remove todas as subscrições de um cliente
void remove_all_subscriptions(const int fifo_fd);

// Verifica se um cliente está subscrito a uma chave
int is_suscribed(const char *key, const int fifo_fd);

#endif  // SUBSCRIPTIONS_H
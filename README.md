# Projeto-SO

Este projeto implementa uma Tabela de Hash (Hash Table) para armazenar pares chave-valor. A tabela de hash é protegida por locks de leitura e escrita para garantir a segurança em ambientes multithread.

## Estrutura do Projeto

- `main.c`: Contém a função principal que inicializa a tabela de hash, lê os comandos dos arquivos `.job` e executa as operações correspondentes.
- `kvs.c` e `kvs.h`: Implementam a tabela de hash e as operações básicas como leitura, escrita, e exclusão de pares chave-valor.
- `operations.c` e `operations.h`: Contêm funções para inicializar e finalizar a tabela de hash, além de funções para mostrar o estado atual da tabela e criar backups.
- `parser.c` e `parser.h`: Implementam funções para ler e interpretar comandos dos arquivos `.job`.
- `utils.c` e `utils.h`: Contêm funções auxiliares para manipulação de locks e ordenação de pares chave-valor.

## Funcionalidades

- **WRITE**: Adiciona ou atualiza pares chave-valor na tabela de hash.
- **READ**: Lê os valores associados às chaves fornecidas.
- **DELETE**: Remove pares chave-valor da tabela.
- **SHOW**: Mostra o estado atual da tabela de hash.
- **BACKUP**: Cria um backup do estado atual da tabela de hash.
- **WAIT**: Espera por um determinado período de tempo.

1. Compile o projeto usando o Makefile:
    ```sh
    make
    ```

2. Execute o programa com o caminho do diretório contendo os arquivos `.job`, o número de backups permitidos e o número de threads:
    ```sh
    ./kvs <directory_path> <number_backups> <number_threads>
    ```


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define LINE_BUFFER 512
#define DATE_PREFIX_LEN 8

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <file.csv> <date_prefix>\n", argv[0]);
        return EXIT_FAILURE;
    }

    long num_procs = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_procs < 1) {
        perror("sysconf");
        return EXIT_FAILURE;
    }
    printf("available processors:\t\t %ld\n", num_procs);

    if (strlen(argv[2]) != DATE_PREFIX_LEN - 1) {
        fprintf(stderr, "Prefix Format: 'YYYY-MM'\n");
        return EXIT_FAILURE;
    }

    /*
    int csv = open(argv[1], O_RDONLY);
    if (csv == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    OU

    FILE *csv = fopen(argv[1], "r");
    if (!csv) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    open() vs fopen()
        - fopen() usar buffer de arquivo para reduzir chamadas de sistema
        - fopen() tem um offset global, as threads não podem compartilhar o mesmo FILE *
        - fopen() vai usar mais memoria, cada thread vai ter seu proprio descritor e buffer
        - open()  realiza uma syscall a cada leitura (pode ser menos eficiente para muitas leituras,
            logo, nesse caso compensa ler blocos bem maiores que o LINE_BUFFER)
    */

    /*
    fseek(csv, 0L, SEEK_END);
    long total_size = ftell(csv);
    printf("file size (bytes):\t\t %ld\n", total_size);

    long chunk = total_size / num_procs;
    printf("~ chunk size (bytes):\t\t %ld\n", chunk);

    char line[LINE_BUFFER];

    rewind(csv);
    fgets(line, sizeof(line), csv); // Header Skip
    long offset = ftell(csv);
    fclose(csv);
    */
    //printf("start offset: %ld\n", offset);

    // iniciar as threads

    /*
    for (int i = 0; i < num_procs; i++)
        Thread 'i'
            offset.start = offset;
            offset.end   = chunk * (i+1);
            offset       = offset.end;
    */

    /* 
    No inicio do processamento da chunck da thread ela deve verificar se o ponteiro do arquivo
    esta no inicio de uma "linha de dados", caso não, deve avançar para a proxima linha e ignorar 
    o processamento da linha anterior. (ALERTA DE POSSIVEIS BUGS)

    Ao final do processamento da chunck da thread ela deve verificar onde terminou o ponteiro,
    se terminou no meio de uma linha ela deve voltar os caracteres ate o inicio da linha e 
    processar essa linha de dados.
    */

    /*
    Processamento interno thread:
        - area compartilhada de memoria para chave "sensor-YYYY-MM" ???
        - qual criterio de controle ???
        - precisa verificar se o prefixo 'YYYY-MM' e valido ???
    */

    // Espera todas as threads terminarem para criar o arquivo de saida ???

    return 0;
}
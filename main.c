#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include <uthash.h> // dependencia externa

#define MAX_DEVICE_LEN 50
#define MAX_YM_LEN 7      // "YYYY-MM"
#define MAX_SENSOR_LEN 14

// Estrutura para armazenar estatisticas
typedef struct {
    char composite_key[MAX_DEVICE_LEN + MAX_YM_LEN + MAX_SENSOR_LEN + 2]; // Chave composta
    char device[MAX_DEVICE_LEN];
    char year_month[MAX_YM_LEN + 1]; // +1 para o terminador nulo '\0'
    char sensor[MAX_SENSOR_LEN];
    double sum;
    double min;
    double max;
    int count;
    UT_hash_handle hh; // Campo interno da uthash (ver documentacao lib)
} StatsEntry;

// Dados passados para cada thread
typedef struct {
    const char *data;               // Ponteiro para os dados mapeados
    size_t start;                   // Offset inicial
    size_t end;                     // Offset final
    StatsEntry *stats;              // Tabela hash local da thread
    const char *year_month_filter;  // Filtro YYYY-MM (argv[2])
    size_t filesize;                // Tamanho total do arquivo
} ThreadData;

// Funcao para processar um bloco do arquivo
void process_region(const char *data, size_t start, size_t end, const char *year_month_filter, size_t filesize, StatsEntry **stats) {
    size_t i = start;

    // Ajusta o inicio para comecar no proximo '\n' (se o chunk resultar no meio de uma linha)
    if (i != 0) {
        while (i < end && data[i] != '\n') i++;
        if (i < end) i++;
    }

    size_t adjusted_end = end;
    if (adjusted_end != filesize) {
        while (adjusted_end < filesize && data[adjusted_end] != '\n') adjusted_end++; // Resolve o problema da quebra de linha no final do chunk
        if (adjusted_end < filesize) adjusted_end++; // Inclui o '\n'
    }

    while (i < adjusted_end) {
        size_t line_start = i;
        size_t line_end = i;

        // Encontra o fim da linha atual (estabele intervalo para processamento)
        while (line_end < adjusted_end && data[line_end] != '\n') line_end++;

        size_t tokens_start[12];
        size_t tokens_len[12];
        int token_count = 0;
        size_t token_start = line_start;

        // Tokenizacao manual (testes anteriores usavam sscanf(), com um custo desnecessario)
        for (size_t pos = line_start; pos <= line_end && token_count < 12; pos++) {
            if (pos == line_end || data[pos] == '|') {
                tokens_start[token_count] = token_start;
                tokens_len[token_count] = pos - token_start;
                token_count++;
                token_start = pos + 1;
            }
        }

        // Processa apenas linhas validas (conhecendo a base de dados linhas com dados faltantes sao irrelevantes para a analise)
        if (token_count >= 12) {
            // Extrai device (token 1)
            char device[MAX_DEVICE_LEN + 1];
            size_t dev_len = tokens_len[1];
            dev_len = dev_len > MAX_DEVICE_LEN ? MAX_DEVICE_LEN : dev_len;
            memcpy(device, data + tokens_start[1], dev_len);
            device[dev_len] = '\0';

            // Extrai e verifica data (token 3) "YYYY-MM"
            char ym_str[8];
            memcpy(ym_str, data + tokens_start[3], 7);
            ym_str[7] = '\0';

            // Filtra datas >= YYYY-MM usando "comparacao direta" (tchau MALDITO sscanf())
            if (strncmp(ym_str, year_month_filter, 7) >= 0) {
                // Processa cada sensor
                const int sensor_indices[] = {4,5,6,7,8,9};
                const char* sensor_names[] = {"temperatura", "umidade", "luminosidade", "ruido", "eco2", "etvoc"};

                for (int s = 0; s < 6; s++) {
                    size_t val_len = tokens_len[sensor_indices[s]];
                    if (val_len == 0) continue;

                    // Conversao direta para double sem sscanf
                    char *endptr;
                    double value = strtod(data + tokens_start[sensor_indices[s]], &endptr);
                    if (endptr == data + tokens_start[sensor_indices[s]]) continue;

                    // Montagem manual da chave composta (mais rapido que snprintf)
                    char composite_key[MAX_DEVICE_LEN + MAX_YM_LEN + MAX_SENSOR_LEN + 2];
                    char *p = composite_key;
                    
                    // Copia device
                    memcpy(p, device, dev_len);
                    p += dev_len;
                    *p++ = '|';
                    
                    // Copia ano-mes
                    memcpy(p, ym_str, 7);
                    p += 7;
                    *p++ = '|';
                    
                    // Copia sensor (nome)
                    const char *sensor = sensor_names[s];
                    size_t sensor_len = strlen(sensor);
                    memcpy(p, sensor, sensor_len);
                    p[sensor_len] = '\0';

                    // Busca ou cria entrada na hash table
                    StatsEntry *entry;
                    HASH_FIND_STR(*stats, composite_key, entry);
                    
                    if (!entry) {
                        entry = malloc(sizeof(StatsEntry));
                        strcpy(entry->composite_key, composite_key);
                        strncpy(entry->device, device, MAX_DEVICE_LEN);
                        strncpy(entry->year_month, ym_str, 7);
                        entry->year_month[7] = '\0';
                        strncpy(entry->sensor, sensor, MAX_SENSOR_LEN);
                        entry->sum = value;
                        entry->count = 1;
                        entry->min = entry->max = value;
                        HASH_ADD_STR(*stats, composite_key, entry);
                    } else {
                        entry->sum += value;
                        entry->count++;
                        if (value < entry->min) entry->min = value;
                        if (value > entry->max) entry->max = value;
                    }
                }
            }
        }

        i = line_end + 1; // Proxima linha
    }
}

// Funcao executada por cada thread
void *thread_func(void *arg) {
    ThreadData *td = (ThreadData *)arg;
    td->stats = NULL;
    process_region(td->data, td->start, td->end, td->year_month_filter, td->filesize, &td->stats);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <arquivo_entrada.csv> <YYYY-MM>\n", argv[0]); // necessario adicionar verificacao de entradas?
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); exit(EXIT_FAILURE); }

    struct stat st;
    if (fstat(fd, &st) < 0) { perror("fstat"); exit(EXIT_FAILURE); }
    size_t filesize = st.st_size;

    // Mapeia o arquivo para memoria (mmap() e a melhor abordagem de acordo com os testes preliminares)
    char *data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { perror("mmap"); exit(EXIT_FAILURE); }

    // Cria threads baseado no numero de processadores
    //int nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    int nthreads = get_nprocs();
    pthread_t threads[nthreads];
    ThreadData td[nthreads];

    size_t chunk = filesize / nthreads;

    // Info
    printf("available processors:\t\t %d\n", nthreads);
    printf("file size (bytes):\t\t %zu\n", filesize);
    printf("~chunk size (bytes):\t\t %zu\n", chunk);

    printf("\nprocessing file...\n");
    for (int i = 0; i < nthreads; i++) {
        td[i].data = data;
        td[i].start = i * chunk;
        td[i].end = (i == nthreads - 1) ? filesize : (i + 1) * chunk;
        td[i].year_month_filter = argv[2]; // Passa o filtro YYYY-MM
        td[i].filesize = filesize;         // Passa o tamanho total
        pthread_create(&threads[i], NULL, thread_func, &td[i]);
    }

    // Aguarda conclusao das threads
    for (int i = 0; i < nthreads; i++) pthread_join(threads[i], NULL);

    // Mescla resultados de todas as threads
    printf("merging hash entries...\n");
    StatsEntry *global_stats = NULL;
    for (int i = 0; i < nthreads; i++) {
        StatsEntry *entry, *tmp;
        // Itera sobre cada entrada da thread
        HASH_ITER(hh, td[i].stats, entry, tmp) {
            StatsEntry *global_entry;
            // Busca na hash table global
            HASH_FIND_STR(global_stats, entry->composite_key, global_entry);
            
            if (global_entry) {
                // Atualiza estatisticas globais
                global_entry->sum += entry->sum;
                global_entry->count += entry->count;
                if (entry->min < global_entry->min) global_entry->min = entry->min;
                if (entry->max > global_entry->max) global_entry->max = entry->max;
                // Remove da thread e libera memoria
                HASH_DEL(td[i].stats, entry);
                free(entry);
            } else {
                // Move entrada para a hash table global
                HASH_DEL(td[i].stats, entry);
                HASH_ADD_STR(global_stats, composite_key, entry);
            }
        }
    }

    // Gera arquivo de saida
    printf("creating output file...\n");
    FILE *of = fopen("results.csv", "w");
    if (!of) { perror("fopen"); exit(EXIT_FAILURE); }

    fprintf(of, "device|ano-mes|sensor|valor_maximo|valor_medio|valor_minimo\n");
    StatsEntry *entry, *tmp;
    // Itera sobre todas as entradas globais e escreve os resultados
    HASH_ITER(hh, global_stats, entry, tmp) {
        double avg = entry->sum / entry->count;
        fprintf(of, "%s|%s|%s|%.1f|%.1f|%.1f\n",
                entry->device,
                entry->year_month,
                entry->sensor,
                entry->max,
                avg,
                entry->min);
        HASH_DEL(global_stats, entry);
        free(entry);
    }
    fclose(of);

    // Limpeza final
    munmap(data, filesize);
    close(fd);
    printf("finished.\n");
    return 0;
}
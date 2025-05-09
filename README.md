# Análise de Sensores IoT com Pthreads

Este projeto realiza análise de dados de sensores IoT utilizando pthreads e técnicas de otimização para processamento eficiente de grandes arquivos. O código implementa uma estratégia de divisão de trabalho por threads, processamento lock-free com estruturas locais, e consolidação final de resultados.

---

## Requisitos Atendidos

- Código em C utilizando `pthreads`;
- Número de threads determinado automaticamente com base na quantidade de processadores disponíveis;
- Distribuição do processamento dos dados entre threads;
- Geração de CSV com estatísticas agregadas por "device", "ano-mes" e "sensor".

---

## Dependência Externa <uthash.h>

Biblioteca **header-only** para implementação de tabelas hash (dicionários) em C. Permite transformar qualquer estrutura em uma tabela hash.
> *"Any C structure can be stored in a hash table using uthash. Just add a `UT_hash_handle` to the structure and choose one or more fields in your structure to act as the key."*

### Utilização no projeto:
```c
typedef struct {
    char composite_key[MAX_KEYSIZE];  // Chave: device|ano-mes|sensor
    char device[50];
    char year_month[8];
    char sensor[20];
    double sum, min, max;
    int count;
    UT_hash_handle hh;                // Campo obrigatório para a UTHash
} StatsEntry;
```
### Operações Principais:
```
// Adicionar item
HASH_ADD_STR(minha_tabela, composite_key, novo_item);

// Buscar item
StatsEntry *item;
HASH_FIND_STR(minha_tabela, "chave_buscada", item);

// Iterar sobre todos os itens
StatsEntry *iter, *tmp;
HASH_ITER(hh, minha_tabela, iter, tmp) {
    // Processar cada item
}
```

### Instalação:

1. Método recomendado (incluir localmente):
   ```bash
   wget https://github.com/troydhanson/uthash/raw/master/src/uthash.h -P include/
   ```
   
3. Instalação global (opcional):
   ```bash
   sudo apt install uthash-dev
   ```
    
**Documentação completa**: [Site Oficial](https://troydhanson.github.io/uthash/)

## Compilação

Para compilar o programa:

```bash
gcc main.c -o main -pthread -O2
```
O parâmetro -O2 (opcional) ativa otimizações importantes para desempenho.

## Execução

Execute o programa com:

```bash
./main <arquivo.csv> <YYYY-MM>
```

Exemplo:

```bash
./main devices.csv '2024-02'
```

## Detalhes Técnicos

## 1. Leitura de Arquivo com `mmap()`

- **Desempenho em grandes volumes**: Evita múltiplas chamadas de sistema (`read()`, `fgets()`) e reduz o número de operações pela ausencia do buffer interno do FILE*.
- **Mapeamento direto**: O conteúdo do arquivo torna-se um array de bytes na memória virtual, permitindo acesso randômico eficiente.
- **Baixa sobrecarga de I/O**: Reduz custos de context switch entre kernel e usuário, além de trazer páginas do arquivo sob demanda de acesso e poder realizar head-ahead para otimizar a leitura trazendo proximas paginas ao indentificar leituras sequenciais.

## 2. Divisão em Chunks para Threads

- O tamanho do arquivo (filesize) é dividido igualmente pelo número de CPUs (get_nprocs()).
- Cada thread recebe um offset inicial e final (start, end) para processar.
- Ajuste de fronteiras:
    - No início do chunk, avança até o próximo \n para pular linha cortada.
    - No fim, estende até incluir a última linha completa antes do próximo chunk.
      
## 3. Processamento Lock-Free por Thread

- Cada thread mantém sua própria tabela hash local (StatsEntry *stats) usando uthash.
- Fluxo de processamento:
    1. Itera linhas dentro do chunk.
    2. Tokeniza manualmente pelo caracter '|' (evitando sscanf e alocação extra).
    3. Filtra registros a partir do prefixo data informado pelo usuario via argumento, processa se ano-mes >= 'YYYY-MM'.
    4. Para cada sensor (temperatura, umidade, luminosidade, ruido, eco2, etvoc):
       - Converte valor com strtod().
       - Compoe chave composta (device|YYYY-MM|sensor).
       - Atualiza ou cria entrada na hash local: soma, count, min, max.

## 4. Consolidação de Estatísticas

Após todas as threads terminarem (pthread_join), a thread principal faz merge das tabelas locais:
    1. Para cada entrada de cada thread:
        - Busca a chave em uma tabela global (global_stats).
        - Se existir: acumula sum e count, ajusta min/max.
        - Caso contrário: adiciona a entrada diretamente na tabela global.
    2. Libera as estruturas locais à medida que são movidas ou consolidadas.

## Geração do CSV de Saída

Abre um arquivo texto `results.csv` em modo de escrita.
- Imprime cabeçalho:
    - fprintf(of, "device|ano-mes|sensor|valor_maximo|valor_medio|valor_minimo\n");
- Itera sobre global_stats, calcula média (sum/count) e escreve linha formatada:
    -  fprintf(of, "%s|%s|%s|%.1f|%.1f|%.1f\n", ...);

## Observações
- As colunas **id**, **latitude** e **longitude** não são consideradas na análise.
- Linhas com dados faltantes são descartadas (token_count < 12).

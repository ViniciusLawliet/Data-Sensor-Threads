# 📊 Análise de Sensores IoT com Pthreads

Este projeto realiza a análise de dados de sensores IoT utilizando programação concorrente com `pthreads`. Os dados são processados **mensalmente para cada dispositivo**, identificando os valores **mínimos**, **médios** e **máximos** de cada sensor.

---

## ✅ Requisitos Atendidos

- Código em C utilizando `pthreads`;
- Número de threads determinado automaticamente com base nos núcleos disponíveis;
- Divisão de trabalho por blocos de bytes do arquivo;
- Consideração de registros a partir de **março de 2024**;
- Geração de CSV com estatísticas por sensor, dispositivo e mês.

---

## 🧪 Compilação

Para compilar o programa:

```bash
gcc -O2 -pthread -o analisador main.c
```


## ▶️ Execução

Execute o programa com:

```bash
./analisador base.csv YYYY-MM
```

Exemplo:

```bash
./analisador sensores.csv 2024-03
```

## 📥 Como o CSV é carregado

O arquivo CSV é lido usando fopen, que permite buffer interno e facilita a leitura por linha. Cada thread abre sua própria instância de leitura para evitar conflitos de ponteiro.

## 🔀 Distribuição entre Threads

- O arquivo é dividido em blocos de bytes proporcionalmente ao número de processadores disponíveis;
- Cada thread recebe um "chunk" (parte) do arquivo para processar;
- Para evitar que linhas sejam cortadas entre threads:
    - No início do chunk, a thread avança até a próxima linha válida;
    - No fim do chunk, ela retrocede até o início da linha anterior.

## 🧠 Processamento por Thread

Cada thread executa:

- Leitura das linhas de seu trecho do arquivo;
- Filtragem dos dados com prefixo de data igual ou posterior ao fornecido;
- Extração de sensores:
    - `temperatura`, `umidade`, `luminosidade`, `ruído`, `eco2`, `etvoc`
- Agrupamento por: `device` + `ano-mes` + `sensor`;
- Cálculo dos valores:
    - `valor_minimo`, `valor_médio`, `valor_maximo`.  

## 📤 Geração do CSV de Saída

Após o término do processamento:

- Os resultados são agregados pela thread principal;
- Um novo CSV é gerado com o formato:      

```bash
    device;ano-mes;sensor;valor_maximo;valor_medio;valor_minimo
```
Este arquivo é salvo no diretório local do programa.

## 💻 Execução das Threads
As threads são criadas com a biblioteca POSIX pthreads, executadas em modo usuário.

## ⚠️ Concorrência
Se for usada uma estrutura global compartilhada entre threads (como um dicionário ou hash map para estatísticas), será necessário:

- Controlar o acesso com mutexes para evitar condições de corrida.

Para simplificação e desempenho, cada thread pode usar sua própria estrutura de agregação local e passar os resultados para a thread principal consolidar ao final.

## 📌 Observações
- As colunas **id**, **latitude** e **longitude** não são consideradas na análise.

- O cabeçalho do CSV é descartado automaticamente.

## 📁 Estrutura do Repositório

```graphql
    📁 projeto-sensores/
    ├── analisador.c               # Código-fonte principal
    ├── README.md                  # Este arquivo
    ├── sensores.csv               # Exemplo de base de entrada (não incluído no repositório)
    └── resultados/
        └── saida.csv              # Arquivo CSV gerado com os resultados
```

## 🔗 Base de Dados
A base utilizada para análise está disponível em:

[📥 Download da base (Google Drive)](https://drive.google.com/file/d/1fEbhm19z0zH6wS7QZU4t8e0WxrPk6awm/view?usp=sharing)


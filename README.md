# Simulador de Escalonamento Round Robin Multinúcleo

## Descrição

Este projeto simula o algoritmo de escalonamento **Round Robin** com suporte a múltiplos núcleos de CPU, operações de E/S (bloqueios) e inserção dinâmica de processos. O objetivo é consolidar conceitos de sistemas operacionais, tais como conceitos de escalonamento round robin e uso de threads.

---

## Funcionalidades

- **Round Robin** com quantum configurável (fixo ou dinâmico).
- **Execução em múltiplos núcleos** (2 ou mais CPUs simuladas por threads).
- **Bloqueio de processos para I/O** e retorno à fila de prontos.
- **Inserção dinâmica de processos** durante a simulação via pipe.
- **Relatórios e linha do tempo** textual estilo Gantt ao final da execução.

---

## Estrutura do Projeto

```
.
├── Escalonador.cpp         # Código principal do simulador
├── InserirProcessos.cpp    # Utilitário para inserir processos dinamicamente
├── Processo.hpp            # Definição da estrutura do processo
├── entrada.txt             # Arquivo de entrada de processos
├── Makefile                # Script de compilação
```

---

## Requisitos
- Sistema Operacional: Linux
- Compilador: gcc

---

## Como Executar

### 1. Compilar

No terminal, execute:
```sh
make
```

### 2. Preparar o arquivo de entrada

Edite o arquivo `entrada.txt` com os processos no formato:
```
ID TEMPO_CHEGADA EXEC1 BLOQUEIO(0/1) ESPERA EXEC2
```
Exemplo:
```
1 0 3 0 0 0
2 1 4 1 2 6
4 8 4 0 0 0
```

### 3. Rodar o simulador

```sh
./escalonador
```
Digite o valor do quantum quando solicitado.

### 4. Inserir processos dinamicamente (opcional)

Em outro terminal, execute:
```sh
./inserir
```
Siga as instruções para enviar novos processos durante a simulação.

---

## Saída Esperada

- **Tabela de métricas**: tempo de espera, turnaround, uso da CPU, trocas de contexto.
- **Linha do tempo textual**: mostra a execução de cada núcleo ao longo do tempo, estilo Gantt.

Exemplo:
```
Tempo:   0   1   2   3   4   5   6
CPU1 (TID: 0x...):   1   1   o   2   2   o   3
CPU2 (TID: 0x...):   o   2   2   o   1   1   o

--- Estatísticas dos Processos ---
Processo[1]: Turnaround = 7, Chegada = 0, Inicio Execucao: 0, Fim Execucao: 7, Tempo de espera = 1, Trocas de contexto = 2
...
```

---

## Critérios de Avaliação

- Simulação de núcleos e escalonador funcional
- Gerenciamento de bloqueios e fila de I/O
- Inserção dinâmica de processos
- Relatório com análise e métricas
- Execução e apresentação coerente

---

**Desenvolvido para fins acadêmicos.**
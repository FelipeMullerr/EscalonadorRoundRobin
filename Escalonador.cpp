#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <sstream>
#include <sys/stat.h>
#include <pthread.h>
#include "Processo.hpp"
#include <iomanip> 

using namespace std;

#define MAX_PROCESSOS 100
#define N_CPUS 2

int QUANTUM = 2, tempo_global = 0;
int linhaExecCPU[N_CPUS][150];

Processo fila_prontos[MAX_PROCESSOS]; int tamanho_fila = 0;
Processo fila_bloqueados[MAX_PROCESSOS]; int tamanho_bloqueados = 0;
Processo lista_processos[MAX_PROCESSOS]; int total_processos = 0;

Processo* executando[N_CPUS] = {nullptr};

pthread_mutex_t mutex_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_sync = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_sync = PTHREAD_COND_INITIALIZER;
int threads_prontas = 0; 
bool ciclo_ativo = false, simulacao_ativa = true;


// ---------- Funçoes para exibir as filas/logs e estatísticas do programa ----------
void exibirFilaProntos() {
    cout << "Fila pronta: [";
    for (int i = 0; i < tamanho_fila; ++i) {
        cout << fila_prontos[i].id;
        if (i < tamanho_fila - 1) cout << ", ";
    }
    cout << "]\n";
}

void exibirFilaBloqueados() {
    cout << "Fila bloqueados: [";
    for (int i = 0; i < tamanho_bloqueados; ++i) {
        cout << fila_bloqueados[i].id << "(" << fila_bloqueados[i].tempo_bloqueio_restante << "s restantes)";
        if (i < tamanho_bloqueados - 1) cout << ", ";
    }
    cout << "]\n";
}

void mostrarLogsExec(pthread_t threads[]) {
    for (int i = 0; i < N_CPUS; i++) {
        cout << "CPU" << (i+1) << " (TID: " << threads[i] << "): ";
        if (executando[i] != nullptr) {
            cout << "executando processo[" << executando[i]->id << "] "
                 << "(tempo restante: " << executando[i]->tempo_restante
                 << "s, quantum restante: " << executando[i]->quantum_restante << ")\n";
        } else {
            cout << "ociosa\n";
        }
    }
    exibirFilaProntos();
    exibirFilaBloqueados();
}

void linhaTempoCPU(pthread_t threads[]) {
    cout << "\nTempo: " << setw(19) << " ";
    for (int i = 0; i < tempo_global; i++) {
        cout << setw(3) << i << " ";
    }
    for (int c = 0; c < N_CPUS; c++) {
        cout << "\nCPU" << (c+1) << " (TID: " << threads[c] << "):  ";
        for (int i = 0; i < tempo_global; i++) {
            if (linhaExecCPU[c][i] == -1) cout << setw(3) << "o" << " ";
            else cout << setw(3) << linhaExecCPU[c][i] << " ";
        }
    }
    cout << "\n";
}

void mostrarEstatisticas(pthread_t threads[]) {
    cout << "\n--- Estatísticas dos Processos ---\n";
    for (int i = 0; i < total_processos; i++) {
        Processo& p = lista_processos[i];

        int turnaround = (p.tempo_fim_execucao == -1) ? -1 : p.tempo_fim_execucao - p.tempo_chegada;
        int tempo_execucao_total = p.exec1 + (p.tem_bloqueio ? p.exec2 : 0);
        int espera = (p.tempo_fim_execucao == -1) ? -1 : turnaround - tempo_execucao_total;

        cout << "Processo[" << p.id << "]: Turnaround = ";
        if (turnaround == -1) cout << "não finalizado";
        else cout << turnaround;
        cout << ", Chegada = " << p.tempo_chegada;
        cout << ", Inicio Execucao: " << p.tempo_inicio_execucao << ", Fim Execucao: " << p.tempo_fim_execucao;
        cout << ", Tempo de espera = ";
        if (espera == -1) cout << "não finalizado";
        else cout << espera;
        cout << ", Trocas de contexto = " << p.trocas_contexto << "\n";
    }

    cout << "\n--- Estatísticas do CPU ---\n";
    linhaTempoCPU(threads);
}

// ---------- Funções para manipulação das filas e processos ----------
void adicionarPronto(Processo p) {
    if (tamanho_fila < MAX_PROCESSOS) {
        fila_prontos[tamanho_fila++] = p;
    } else {
        cout << "Fila pronta cheia!\n";
    }
}

Processo removerPronto() {
    Processo p = fila_prontos[0];
    for (int i = 1; i < tamanho_fila; i++) {
        fila_prontos[i-1] = fila_prontos[i];
    }
    tamanho_fila--;
    return p;
}

// ---------- Funções para carregar processos do arquivo e pipe ----------
void carregarProcessosDeArquivo(const char* nomeArquivo) {
    ifstream arq(nomeArquivo);
    if (!arq.is_open()) {
        cout << "Arquivo " << nomeArquivo << " não encontrado.\n";
        return;
    }

    string linha;
    while (getline(arq, linha)) {
        if (linha.empty()) continue;
        stringstream ss(linha);
        Processo p;
        int bloqueio_int;

        if (!(ss >> p.id >> p.tempo_chegada >> p.exec1 >> bloqueio_int >> p.espera >> p.exec2)) {
            cout << "Linha inválida no arquivo: " << linha << "\n";
            continue;
        }

        p.tem_bloqueio = (bloqueio_int == 1);
        p.estado = PRONTO;
        p.tempo_restante = p.exec1;
        p.quantum_restante = QUANTUM;
        p.tempo_bloqueio_restante = 0;
        p.tempo_inicio_execucao = -1;
        p.tempo_fim_execucao = -1;
        p.tempo_espera = 0;
        p.trocas_contexto = 0;
        p.fase_exec2_iniciada = false;

        if (total_processos < MAX_PROCESSOS) {
            lista_processos[total_processos++] = p;
        } else {
            cout << "Limite máximo de processos atingido!\n";
        }
    }
    arq.close();
}

void verificarChegadaProcessosPipe() {
    for (int i = 0; i < total_processos; i++) {
        if (lista_processos[i].estado == PRONTO && lista_processos[i].tempo_chegada == tempo_global) {
            adicionarPronto(lista_processos[i]);
            cout << "Evento: processo[" << lista_processos[i].id << "] chegou no tempo " << tempo_global << " e entrou na fila pronta\n";
        }
    }
}

// ---------- Função para atribuir processos às CPUs (threads) ----------
void atribuir_processo(Processo*& executando, int cpu_id) {
    pthread_mutex_lock(&mutex_exec);
    if (executando == nullptr && tamanho_fila > 0) {
        Processo p = removerPronto();
        executando = new Processo(p);
        executando->estado = EXECUTANDO;
        executando->trocas_contexto++;
        executando->quantum_restante = QUANTUM;
        if (executando->tempo_inicio_execucao == -1) {
            executando->tempo_inicio_execucao = tempo_global;
        }
        cout << "Evento: CPU" << (cpu_id+1) << " iniciou processo[" << executando->id << "] (tempo restante " << executando->tempo_restante << ", quantum " << executando->quantum_restante << ")\n";
    }
    pthread_mutex_unlock(&mutex_exec);
}

// ---------- Funções para manipulação dos processos bloqueados ----------
void atualizarBloqueados() {
    for (int i = 0; i < tamanho_bloqueados; ) {
        fila_bloqueados[i].tempo_bloqueio_restante--;
        cout << "Evento: processo[" << fila_bloqueados[i].id << "] bloqueado, resta " << fila_bloqueados[i].tempo_bloqueio_restante << " unidades\n";

        if (fila_bloqueados[i].tempo_bloqueio_restante <= 0) {
            Processo p = fila_bloqueados[i];
            p.estado = PRONTO;
            p.fase_exec2_iniciada = true;
            p.tempo_restante = p.exec2;
            p.quantum_restante = QUANTUM;

            adicionarPronto(p);

            for (int j = i + 1; j < tamanho_bloqueados; j++) {
                fila_bloqueados[j - 1] = fila_bloqueados[j];
            }
            tamanho_bloqueados--;
            cout << "Evento: processo[" << p.id << "] desbloqueado e voltou para fila pronta (2 tempo de execucao)\n";
        } else {
            i++;
        }
    }
}

// ---------- Funcao para atualizar o processo original na lista de processos ----------
void atualizarOriginal(Processo* modificado) {
    for (int i = 0; i < total_processos; i++) {
        if (lista_processos[i].id == modificado->id) {
            lista_processos[i] = *modificado;
            break;
        }
    }
}

// ---------- Funcao para ler novos processos vindos do pipe (insercao dinamica) ----------
void lerPipe(int pipe_fd) {
    char buffer[256];

    ssize_t bytes_lidos = read(pipe_fd, buffer, sizeof(buffer) - 1);

    if (bytes_lidos > 0) {
        buffer[bytes_lidos] = '\0';

        Processo novo;
        int bloqueio_int;
        std::stringstream ss(buffer);
        if (!(ss >> novo.id >> novo.tempo_chegada >> novo.exec1 >> bloqueio_int >> novo.espera >> novo.exec2)) {
            cout << "Formato inválido no processo recebido: " << buffer << "\n";
            return;
        }

        novo.tem_bloqueio = (bloqueio_int == 1);
        novo.estado = PRONTO;
        novo.tempo_restante = novo.exec1;
        novo.quantum_restante = QUANTUM;
        novo.tempo_bloqueio_restante = 0;
        novo.tempo_inicio_execucao = -1;
        novo.tempo_fim_execucao = -1;
        novo.tempo_espera = 0;
        novo.trocas_contexto = 0;
        novo.fase_exec2_iniciada = false;

        if (total_processos < MAX_PROCESSOS) {
            lista_processos[total_processos++] = novo;
            if (novo.tempo_chegada <= tempo_global) {
                adicionarPronto(novo);
                cout << "Evento: processo[" << novo.id << "] inserido dinamicamente e entrou na fila pronta\n";
                cout << "Processo info - id: " << novo.id << ", tempo_chegada: " << novo.tempo_chegada << ", exec1: " << novo.exec1 << ", bloqueio: " << novo.tem_bloqueio << ", espera: " << novo.espera << ", exec2: " << novo.exec2 << "\n";
            } else {
                cout << "Evento: processo[" << novo.id << "] inserido dinamicamente com chegada futura\n";
            }
        } else {
            cout << "Limite máximo de processos atingido!\n";
        }
    }
}

// ---------- Funcao para controle de quantum e execucao para cada CPU (Thread) ----------
void* thread_cpu(void* arg) {
    int cpu_id = *((int*)arg);

    while (true) {
        // Esoera o ciclo ser ativo pela funcao SimularRoundRobin (Para poder rodar o ciclo de execucao)
        pthread_mutex_lock(&mutex_sync);
        while (!ciclo_ativo && simulacao_ativa) {
            pthread_cond_wait(&cond_sync, &mutex_sync);
        }
        // Se a simulacao acabou, entao ele termina o loop de execucao
        if (!simulacao_ativa) {
            pthread_mutex_unlock(&mutex_sync);
            break;
        }
        pthread_mutex_unlock(&mutex_sync);

        // Lock na regiao critica de acesso a memoria (vetor de processos em execucao)
        pthread_mutex_lock(&mutex_exec);
        if (executando[cpu_id]) {
            // se o processo terminou a execucao da primeira fase
            if (executando[cpu_id]->tempo_restante == 0) {
                // Verifica se o processo precisa ser bloqueado por x tempo ou se deve ser finalizado
                if (executando[cpu_id]->tem_bloqueio && !executando[cpu_id]->fase_exec2_iniciada) {
                    executando[cpu_id]->estado = BLOQUEADO;
                    executando[cpu_id]->tempo_bloqueio_restante = executando[cpu_id]->espera;
                    atualizarOriginal(executando[cpu_id]);
                    if (tamanho_bloqueados < MAX_PROCESSOS) {
                        fila_bloqueados[tamanho_bloqueados++] = *executando[cpu_id];
                        cout << "Evento: processo[" << executando[cpu_id]->id << "] bloqueado por " << executando[cpu_id]->espera << " unidades\n";
                    } else {
                        cout << "Fila bloqueados cheia!\n";
                    }
                    delete executando[cpu_id];
                    executando[cpu_id] = nullptr;
                } else {
                    // Se nao tiver bloqueio e ja acabou a primeira fase, finaliza o processo
                    executando[cpu_id]->estado = FINALIZADO;
                    executando[cpu_id]->tempo_fim_execucao = tempo_global;
                    cout << "Evento: processo[" << executando[cpu_id]->id << "] finalizado no tempo " << tempo_global << "\n";
                    atualizarOriginal(executando[cpu_id]);
                    delete executando[cpu_id];
                    executando[cpu_id] = nullptr;
                }
                // Se o quantum acabou, move novamente o processo para a fila de PRONTO
            } else if (executando[cpu_id]->quantum_restante == 0) {
                executando[cpu_id]->estado = PRONTO;
                executando[cpu_id]->quantum_restante = QUANTUM;
                cout << "Evento: quantum do processo[" << executando[cpu_id]->id << "] esgotado, volta para fila de pronto (faltam " << executando[cpu_id]->tempo_restante << " tempos para terminar a execução)\n";
                atualizarOriginal(executando[cpu_id]);
                adicionarPronto(*executando[cpu_id]);
                delete executando[cpu_id];
                executando[cpu_id] = nullptr;
            }
        }
        pthread_mutex_unlock(&mutex_exec);

        // Envia o sinal para o SimularRoundRobin de que a thread terminou o ciclo de execucao
        pthread_mutex_lock(&mutex_sync);
        threads_prontas++;
        // Se todas as threads terminaram, envio o sinal para o SimularRoundRobin continuar
        if (threads_prontas == N_CPUS) {
            pthread_cond_signal(&cond_sync);
        }
        while (ciclo_ativo && simulacao_ativa) {
            pthread_cond_wait(&cond_sync, &mutex_sync);
        }
        pthread_mutex_unlock(&mutex_sync);

        if (!simulacao_ativa) break;
    }
    return nullptr;
}

// ---------- Funçoes para inicializar as threads e para finalizar simulacao ----------
void inicializarCPUs(pthread_t threads[], int ids[]) {
    for (int i = 0; i < N_CPUS; i++) {
        ids[i] = i;
        pthread_create(&threads[i], nullptr, thread_cpu, &ids[i]);
    }
}

void finalizarSimulacao(int pipe_fd, pthread_t threads[], int n) {
    cout << "Nenhum processo para executar. Finalizando simulador.\n";
    mostrarEstatisticas(threads);
    close(pipe_fd);
    for (int i = 0; i < n; i++) {
        cout << endl;
        cout << "Aguardando thread CPU" << (i) << " finalizar...\n";
        pthread_join(threads[i], nullptr);
        cout << "Thread CPU" << (i) << " finalizada.\n";
    }
    exit(0);
}

// ---------- Funcao principal que simula o escalonador e realiza as demais verificacoes de filas ----------
void simularRoundRobin(int pipe_fd) {
    pthread_t threads[N_CPUS];
    int ids[N_CPUS];
    inicializarCPUs(threads, ids);

    while (true) {
        cout << "\nTempo[" << tempo_global << "] --------------\n";
        // Faz a leitura do pipe para novos processos, verificacao da chegada destes processos e ciclo de logica dos processos bloqueados
        lerPipe(pipe_fd);
        verificarChegadaProcessosPipe();
        atualizarBloqueados();

        // Atribui os processos que estao na lista de PRONTO para as CPUs que estao ociosas
        for (int i = 0; i < N_CPUS; i++) {
            atribuir_processo(executando[i], i);
        }

        // Habilita o ciclo_ativo para que as threads possam executar mais um ciclo
        pthread_mutex_lock(&mutex_sync);
        ciclo_ativo = true;
        // Envia um sinal para que todas as threads iniciem o ciclo
        pthread_cond_broadcast(&cond_sync);
        pthread_mutex_unlock(&mutex_sync);

        // Aguarda as threads terminarem o ciclo de execucao
        pthread_mutex_lock(&mutex_sync);
        while (threads_prontas < N_CPUS) {
            pthread_cond_wait(&cond_sync, &mutex_sync);
        }
        threads_prontas = 0;
        ciclo_ativo = false;
        // Envia o sinal para as threads voltaram para o estado de espera pelo proximo ciclo ativo
        pthread_cond_broadcast(&cond_sync);
        pthread_mutex_unlock(&mutex_sync);

        // Mostrar os logs do andamento da execucao das threads e status de cada lista
        mostrarLogsExec(threads);

        // Atualizacao da linha do tempo de execucao de cada CPU
        for (int i = 0; i < N_CPUS; i++) {
            linhaExecCPU[i][tempo_global] = executando[i] ? executando[i]->id : -1;
        }
        // Decremento do tempo_restante e quantum_restante dos processos que estao em execucao
        for (int i = 0; i < N_CPUS; i++) {
            if (executando[i]) {
                executando[i]->tempo_restante--;
                executando[i]->quantum_restante--;
            }
        }

        tempo_global++;
        sleep(1);

        // Verifica se todos os processos terminaram a sua execucao e se todas as filas estao vaziaas
        bool todos_ociosos = (tamanho_fila == 0 && tamanho_bloqueados == 0);
        for (int i = 0; i < N_CPUS; i++) {
            if (executando[i] != nullptr) {
                todos_ociosos = false;
                break;
            }
        }
        if (todos_ociosos) {
            // Sinaliza para as threads que a simulacao acabou
            pthread_mutex_lock(&mutex_sync);
            simulacao_ativa = false;
            pthread_cond_broadcast(&cond_sync);
            pthread_mutex_unlock(&mutex_sync);

            finalizarSimulacao(pipe_fd, threads, N_CPUS);
            break;
        }
    }
}

int main() {
    cout << "Iniciando simulador Round Robin com " << N_CPUS << " CPUs (threads)...\n";
    cout << "Digite o QUANTUM (padrão 2): ";
    cin >> QUANTUM;
    if (QUANTUM <= 0) {
        cout << " QUANTUM inválido, usando valor padrão 2.\n";
        QUANTUM = 2;
    } else {
        cout << "QUANTUM definido como: " << QUANTUM << "\n";
    }

    mkfifo("fifo_processos", 0666);
    carregarProcessosDeArquivo("entrada.txt");

    int pipe_fd = open("fifo_processos", O_RDONLY | O_NONBLOCK);
    if (pipe_fd == -1) {
        perror("Erro ao abrir fifo_processos");
        return 1;
    } else {
        cout << "Pipe fifo_processos aberto para leitura.\n";
    }

    simularRoundRobin(pipe_fd);

    return 0;
}
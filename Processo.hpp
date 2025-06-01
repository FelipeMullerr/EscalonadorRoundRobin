#ifndef PROCESSO_HPP
#define PROCESSO_HPP

#define MAX_PROCESSOS 100

enum Estado {
    PRONTO,
    EXECUTANDO,
    BLOQUEADO,
    FINALIZADO
};

struct Processo {
    int id;
    int tempo_chegada;
    int exec1;
    bool tem_bloqueio;
    int espera;
    int exec2;

    int tempo_restante;
    int quantum_restante;
    int tempo_bloqueio_restante;

    Estado estado;

    int tempo_inicio_execucao;
    int tempo_fim_execucao;
    int tempo_espera;
    int trocas_contexto;

    bool fase_exec2_iniciada;
};

#endif

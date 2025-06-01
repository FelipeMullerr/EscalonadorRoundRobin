#include <iostream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
using namespace std;

int main() {
    while (true) {
        string id, chegada, exec1, bloqueio, espera, exec2;

        cout << "Digite novo processo no formato:\n";
        cout << "id tempo_chegada exec1 bloqueio(0 ou 1) espera exec2\n";
        cout << "(ou digite 'sair' para encerrar)\n";
        cin >> id;
        if (id == "sair") break;
        cin >> chegada >> exec1 >> bloqueio >> espera >> exec2;

        string dados = id + " " + chegada + " " + exec1 + " " + bloqueio + " " + espera + " " + exec2 + "\n";

        int fd = open("fifo_processos", O_WRONLY);
        if (fd == -1) {
            perror("Erro ao abrir fifo_processos para escrita");
            return 1;
        }

        write(fd, dados.c_str(), dados.length());
        close(fd);

        cout << "Processo enviado!\n";
    }
    return 0;
}
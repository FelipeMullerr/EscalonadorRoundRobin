all: escalonador inserir

escalonador: Escalonador.cpp
	g++ -o escalonador Escalonador.cpp -pthread

inserir: InserirProcessos.cpp
	g++ -o inserir InserirProcessos.cpp -pthread

clean:
	rm -f escalonador inserir

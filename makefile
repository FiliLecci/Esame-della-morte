.PHONY = all clean
CC = gcc
FLAGS = -Wall -g
LIB = -lpthread

all:
	clear
	$(CC) $(FLAGS) unboundedqueue/unboundedqueue.c bib_client.c -o client $(LIB)
	$(CC) $(FLAGS) unboundedqueue/unboundedqueue.c server_biblio.c -o server $(LIB)

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./client.o --autore="shiau" -p

valgrinds:
	valgrind --leak-check=full --show-leak-kinds=all -s ./server.o bib bibData/bib1.txt 1

run:
	./client --autore="Di Ciccio, Antonio" --editore="Palestro" -p
.SILENT: run

runs:
	./server bib bibData/bib1.txt 1
	
gdb:
	gdb ./server --args server.o bib bibData/bib1.txt 1

clean:
	rm -f *t.o
	rm -f *.conf
	rm -f *.log
	clear
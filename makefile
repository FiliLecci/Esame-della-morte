.PHONY = all clean
CC = gcc
FLAGS = -Wall -g
LIB = -lpthread

all:
	clear
	$(CC) $(FLAGS) unboundedqueue/unboundedqueue.c bib_client.c -o client.o $(LIB)
	$(CC) $(FLAGS) unboundedqueue/unboundedqueue.c server_biblio.c -o server.o $(LIB)

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./client.o --autore="shiau" -p

valgrinds:
	valgrind --leak-check=full --show-leak-kinds=all -s ./server.o bib bibData/bib1.txt 1

run:
	./client.o --titolo="ciao" --autore="pippo" --titolo="pollo" -p

runs:
	./server.o bib bibData/bib1.txt 1
	
gdb:
	gdb ./server.o --args server.o bib bibData/bib1.txt 1

clean:
	rm -f client.o
	rm -f server.o
	clear
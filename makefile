.PHONY = all clean
CC = gcc
FLAGS = -Wall -g
LIB = -D_XOPEN_SOURCE=700

all:
	clear
	$(CC) $(FLAGS) bib_client.c -o client.o $(LIB)
	$(CC) $(FLAGS) server_biblio.c -o server.o $(LIB)

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./client.o --autore="shiau" -p

valgrinds:
	valgrind --leak-check=full --show-leak-kinds=all -s ./server.o bib bibData/bib1.txt 1

run:
	./client.o --titolo="ciao" --autore="pippo" -p
	
gdb:
	gdb ./server.o --args server.o bib bibData/bib1.txt 1

clean:
	rm -f client.o
	rm -f server.o
	clear
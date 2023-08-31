.PHONY = all clean
CC = gcc
FLAGS = -Wall -g
LIB = -lpthread

all:
	clear
	$(CC) $(FLAGS) unboundedqueue/unboundedqueue.c bib_client.c -o client.o $(LIB)
	$(CC) $(FLAGS) unboundedqueue/unboundedqueue.c server_biblio.c -o server.o $(LIB)

valgrind:
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all --num-callers=30 --error-limit=no -s ./client.o --autore="Di Ciccio, Antonio" -p

valgrinds:
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all -s ./server.o bib bibData/bib1.txt 1

run:
	./client.o --autore="Di Ciccio, Antonio" -p
	./client.o --anno="1990"
	./client.o --autore="Di Ciccio, Antonio" --luogo_pubblicazione="Milano"
	./client.o --autore="Di Ciccio, Antonio"

runs:
	./server.o bib bibData/bib1.txt 5
	
gdb:
	gdb ./server.o --args server.o bib bibData/bib2.txt 1

clean:
	rm -f *t.o
	rm -f *.conf
	rm -f *.log
	clear
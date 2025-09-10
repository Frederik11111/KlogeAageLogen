CC=gcc
CFLAGS=-std=c11 -Wall -Werror -Wextra -pedantic -g

myProgram: myProgram.c
	$(CC) -o myProgram myProgram.c $(CFLAGS)

mynameis: mynameis.c
	$(CC) -o mynameis mynameis.c $(CFLAGS)

repeatme: repeatme.c
	$(CC) -o repeatme repeatme.c $(CFLAGS)

noAs: noAs.c
	$(CC) -o noAs noAs.c $(CFLAGS)

guessingGame: guessingGame.c
	$(CC) -o guessingGame guessingGame.c $(CFLAGS)

# quick and dirty makefile
CFLAGS = -std=gnu99 -pedantic -Wall
CFLAGS += -pthread
# CFLAGS += -g

all: thomas

thomas: thomas.c user.o admin.o
	gcc $(CFLAGS) user.o admin.o thomas.c -o thomas

user.o: user.c user.h
	gcc $(CFLAGS) -c user.c

admin.o: admin.c admin.h
	gcc $(CFLAGS) -c admin.c

clean:
	rm -f thomas user.o admin.o

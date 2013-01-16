
CC = g++
CPPFLAGS += -O3 -lpthread -lm

all: main

.PHONY: main

main: threadpool.h threadpool.cpp main.cpp
	$(CC) $(CPPFLAGS) threadpool.cpp main.cpp -o $@

clean:
	rm -f main

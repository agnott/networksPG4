CMP = gcc
FLAGS = -lssl -lcrypto -g -Wall
FNAME = myftp

all: server

server: $(FNAME).c
	$(CMP) $(FLAGS) $(FNAME).c -o myftp

clean:
	rm -f $(FNAME)

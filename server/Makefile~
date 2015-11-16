CMP = gcc
FLAGS = -lssl -lcrypto -g -Wall
FNAME = server

all: server

server: $(FNAME).c
	$(CMP) $(FLAGS) $(FNAME).c -o $(FNAME)

clean:
	rm -f $(FNAME)

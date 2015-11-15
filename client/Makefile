CMP = gcc
FLAGS = -lssl -lcrypto -g -Wall
FNAME = client

all: client

client: $(FNAME).c
	$(CMP) $(FLAGS) $(FNAME).c -o $(FNAME)

clean: 
	rm -f $(FNAME) SmallFile.txt MediumFile.pdf LargeFile.pdf

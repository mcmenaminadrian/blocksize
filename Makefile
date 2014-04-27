default: all

all: blocksize 

clean:
	rm -f *.o

blocksize: blocksize.o
	g++ -O2 -o blocksize -Wall blocksize.o -lexpat -lpthread

blocksize.o: blocksize.cpp
	g++ -O2 -o blocksize.o -c -Wall blocksize.cpp

debug: dblocksize.o
	g++ -g -o blocksize -Wall dblocksize.o -lexpat -lpthread

dblocksize.o: blocksize.cpp
	g++ -g -o dblocksize.o -c -Wall blocksize.cpp

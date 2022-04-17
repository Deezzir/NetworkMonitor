CC=g++
CFLAGS=-I
CFLAGS+=-Wall
FILE1=interfaceMonitor.cpp
FILE2=networkMonitor.cpp

interfaceMonitor: $(FILE1)
	$(CC) $(CFLAGS) $^ -o $@ 

networkMonitor: $(FILE2)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o interfaceMonitor networkMonitor

all: interfaceMonitor networkMonitor
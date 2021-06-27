CC=gcc
CFLAGS=-Wall -Wformat=0 -lcrypt -g -pthread
ODIR=obj
IDIR=../include
LDIR=../lib
PROGRAM=funserver funserver_thread press_test funserver_threadpool funserver_threadpool_epoll

_DEPS = 
DEPS = $(patsubst %, $(IDIR)/%,$(_DEPS))

_OBJ = error_functions.o get_num.o read_line.o sbuf.o inet_sockets.o
OBJ = $(patsubst %, $(ODIR)/%,$(_OBJ))

all : $(PROGRAM)

$(PROGRAM): $(patsubst %, $(ODIR)/%,$(addsuffix .o,$(PROGRAM))) $(OBJ)
	$(CC) -o $@ $(ODIR)/$@.o $(OBJ) $(CFLAGS)

$(ODIR)/%.o: $(LDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean
clean:
	rm -f $(ODIR)/*.o $(PROGRAM) *.o

.PHONY: cleanall
cleanall:
	rm -f $(ODIR)/*.o $(PROGRAM) *.o *.c
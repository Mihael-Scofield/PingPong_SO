# Makefile

# Mihael Scofield de Azevedo - GRR20182621 - msa18

CC = gcc
CFLAGS = -Wall -g # Flags para compilação.
DEBUGFLAGS = -DDEBUG
EXENAMEFLAGS = -o
LDLIBS = -lm
# LDLIBS2 -lSDL2 -Wextra

arq = ppos_core.c queue.c pingpong-mqueue.c
objects = ppos_core.o queue.o pingpong-mqueue.o
exename = exe-pingpong-mqueue

# Regra default
all: Main

# Remove arquivos temporários
clean:
	-rm -f *~ *.o

# Remove tudo o que não for código-fonte original
purge: clean
	-rm -f Main

debug: $(objects)
	$(CC) $(CFLAGS) $(arq) $(DEBUGFLAGS) $(EXENAMEFLAGS) $(exename) $(LDLIBS)

Main: $(objects)
	$(CC) $(CFLAGS) $(arq) $(EXENAMEFLAGS) $(exename) $(LDLIBS)
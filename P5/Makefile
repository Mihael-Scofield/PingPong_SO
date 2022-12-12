# Makefile

# Mihael Scofield de Azevedo - GRR20182621 - msa18

CC = gcc
CFLAGS = -Wall -g -Wextra # Flags para compilação.
DEBUGFLAGS = -DDEBUG
EXENAMEFLAGS = -o
# LDLIBS = -lSDL2 -lSDL2_gfx -lm

arq = ppos_core.c queue.c pingpong-preempcao.c
objects = ppos_core.o queue.o pingpong-preempcao.c
exename = exe-pingpong-preempcao

# Regra default
all: Main

# Remove arquivos temporários
clean:
	-rm -f *~ *.o

# Remove tudo o que não for código-fonte original
purge: clean
	-rm -f Main

debug: $(objects)
	$(CC) $(CFLAGS) $(arq) $(DEBUGFLAGS) $(EXENAMEFLAGS) $(exename)

Main: $(objects)
	$(CC) $(CFLAGS) $(arq) $(EXENAMEFLAGS) $(exename)
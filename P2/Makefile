# Makefile

CC = gcc
CFLAGS = -Wall -g -Wextra # Flags para compilação.
DEBUGFLAGS = -DDEBUG
# LDLIBS = -lSDL2 -lSDL2_gfx -lm

arq = ppos_core.c pingpong-tasks1.c
objects = ppos_core.o pingpong-tasks1.o

# Regra default
all: Main

# Remove arquivos temporários
clean:
	-rm -f *~ *.o

# Remove tudo o que não for código-fonte original
purge: clean
	-rm -f Main

debug: $(objects)
	$(CC) $(CFLAGS) $(arq) $(DEBUGFLAGS)

Main: $(objects)
	$(CC) $(CFLAGS) $(arq)
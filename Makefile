# Makefile - Padrão Modular C (DINF/UFPR)
CC       = gcc
CFLAGS   = -Wall -Wextra -Werror -g -std=c99 -I include
MAIN     = gbv
ENTREGA  = $(MAIN)

BINDIR   = bin
BUILDDIR = build
SRCDIR   = src
INCDIR   = include

# Lista de arquivos de cabeçalho
HDR = $(wildcard $(INCDIR)/*.h)

# Lista de arquivos-objeto gerados a partir do src/
OBJ = $(BUILDDIR)/main.o \
      $(BUILDDIR)/gbv.o \
      $(BUILDDIR)/util.o

# Construir o executável (alvo padrão)
$(BINDIR)/$(MAIN): $(OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^

# Garante que os diretórios de saída existam
$(BINDIR) $(BUILDDIR):
	mkdir -p $@

# Regra genérica para compilar fontes em objetos
$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(HDR) | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Testar com Valgrind para checagem estrita de vazamento de memória
valgrind: $(BINDIR)/$(MAIN)
	valgrind --leak-check=full --track-origins=yes ./$(BINDIR)/$(MAIN)

# Limpeza de binários e objetos
clean:
	rm -f $(OBJ) $(BINDIR)/$(MAIN) $(ENTREGA).tgz
	rm -rf $(BINDIR) $(BUILDDIR)

.PHONY: run valgrind compile_commands tgz clean

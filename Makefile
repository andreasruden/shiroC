CC := gcc
CFLAGS = -Wall -Wextra -std=c23 -pedantic
DEBUGFLAGS = -g -O0
LD := ld

BUILDDIR := build
BINDIR = $(BUILDDIR)/bin
SRCDIR := src

all: $(BINDIR)/lexer

$(BUILDDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c -o $@ $<

$(BINDIR)/lexer: $(BUILDDIR)/lexer.o | $(BINDIR)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o $@ $^

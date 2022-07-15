# Dead-simple makefile for Linux building
CC=gcc
CFLAGS=-I. -Wall
# The MS Windows SDK doesn't have getopt, but Linux/Unix does, so we're not including it on Linux
#DEPS = convfont.h getopt.h parse_fnt.h serialize_font.h
DEPS = convfont.h parse_fnt.h parse_text.h serialize_font.h
#OBJ = convfont.o getopt.o parse_fnt.o serialize_font.o
OBJ = convfont.o parse_fnt.o parse_text.o serialize_font.o

ifeq ($(OS),Windows_NT)
RM = del /f $1 2>nul
EXECUTABLE = convfont.exe
SHELL = cmd.exe
else
EXECUTABLE = convfont
RM = rm -rf $1
endif

all: $(EXECUTABLE)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXECUTABLE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	$(call RM,*.o $(EXECUTABLE))


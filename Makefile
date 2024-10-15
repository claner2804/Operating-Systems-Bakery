# Name of the binary
BINARY = bakery
# Object files
OBJS = bakery.o
# Compiler for C (gcc for C, instead of g++ for C++)
CC = gcc
# Compiler flags
CFLAGS = -Wall -Werror -pthread -g
# Linker flags
LFLAGS =

# All target: builds all important targets
all: $(BINARY)

# Links the binary
$(BINARY): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS)

# Compiles a source file into an object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rules can not only be used for compiling a program but also for executing a program
run: $(BINARY)
	./$(BINARY)

# Delete all build artifacts
clean:
	rm -rf $(BINARY) $(OBJS)

# All and clean are "phony" targets, meaning they are not files
.PHONY: all clean run

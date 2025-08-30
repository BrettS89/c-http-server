CC = gcc
CFLAGS = -I. -Wall
SRCS = $(shell find . -name "*.c")
OBJS = $(SRCS:.c=.o)
EXEC = server

all: $(EXEC)
	rm -f $(OBJS)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC)

clean:
	rm -f $(OBJS) $(EXEC)

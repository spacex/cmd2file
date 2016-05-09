all: cmd2file

LIBS = -pthread

cmd2file: main.c
	$(CC) $< $(LIBS) -o $@

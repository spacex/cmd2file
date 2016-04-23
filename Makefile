all: cmd2file

cmd2file: main.c
	$(CC) $< -o $@

CC = gcc
CFLAGS = -Wall

spellChkr:
	$(CC) $(CFLAGS) myshll.c -o myshll

clean:
	rm -rf *.o myshll

CC = gcc
CFLAGS = -Wall -Wextra -g -pthread -fsanitize=address,undefined
SRC = $(wildcard src/*.c)

proxy: $(SRC)
	$(CC) $(CFLAGS) -o proxy $(SRC)

clean:
	rm -f proxy

tsan: $(SRC)
	$(CC) -Wall -Wextra -g -pthread -fsanitize=thread -o proxy_tsan $(SRC)

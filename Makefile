CC=gcc
CFLAGS=-O2 -Wall
LDFLAGS=-lcrypto

all: aes_flac_key

aes_flac_key: aes_flac_key.c
	$(CC) $(CFLAGS) -o $@ aes_flac_key.c $(LDFLAGS)

clean:
	rm -f aes_flac_key

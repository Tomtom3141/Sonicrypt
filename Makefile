CC=gcc
CFLAGS=-O2 -Wall
LDFLAGS=-lcrypto -lcurl -lcjson

all: aes_flac_key

aes_flac_key: aes_flac_key.c spotify.c spotify.h
	$(CC) $(CFLAGS) -o $@ aes_flac_key.c spotify.c $(LDFLAGS)

clean:
	rm -f aes_flac_key

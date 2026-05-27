#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include "spotify.h"

#define KEY_LEN 32 /* AES-256 */
#define IV_LEN 16
#define BUF_SIZE 4096

static int derive_key_from_file(const char *flac_path, unsigned char *key, size_t key_len) {
    FILE *f = fopen(flac_path, "rb");
    if (!f) return 0;
    SHA256_CTX sha;
    unsigned char buf[BUF_SIZE];
    size_t r;
    SHA256_Init(&sha);
    while ((r = fread(buf, 1, sizeof(buf), f)) > 0) {
        SHA256_Update(&sha, buf, r);
    }
    fclose(f);
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &sha);
    if (key_len > SHA256_DIGEST_LENGTH) return 0;
    memcpy(key, digest, key_len);
    return 1;
}

static int encrypt_file(const char *flac_path, const char *in_path, const char *out_path) {
    unsigned char key[KEY_LEN];
    if (!derive_key_from_file(flac_path, key, KEY_LEN)) {
        fprintf(stderr, "Failed to derive key from FLAC file '%s'\n", flac_path);
        return 0;
    }

    FILE *fin = fopen(in_path, "rb");
    if (!fin) { perror("fopen input"); return 0; }
    FILE *fout = fopen(out_path, "wb");
    if (!fout) { perror("fopen output"); fclose(fin); return 0; }

    unsigned char iv[IV_LEN];
    if (!RAND_bytes(iv, IV_LEN)) { fprintf(stderr, "Failed to generate IV\n"); fclose(fin); fclose(fout); return 0; }
    if (fwrite(iv, 1, IV_LEN, fout) != IV_LEN) { perror("fwrite iv"); fclose(fin); fclose(fout); return 0; }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { fprintf(stderr, "EVP_CIPHER_CTX_new failed\n"); fclose(fin); fclose(fout); return 0; }

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) { fprintf(stderr, "EncryptInit failed\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); return 0; }

    unsigned char inbuf[BUF_SIZE];
    unsigned char outbuf[BUF_SIZE + EVP_CIPHER_block_size(EVP_aes_256_cbc())];
    int inlen, outlen;
    while ((inlen = fread(inbuf, 1, BUF_SIZE, fin)) > 0) {
        if (1 != EVP_EncryptUpdate(ctx, outbuf, &outlen, inbuf, inlen)) { fprintf(stderr, "EncryptUpdate failed\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); return 0; }
        if (fwrite(outbuf, 1, outlen, fout) != (size_t)outlen) { perror("fwrite"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); return 0; }
    }
    if (1 != EVP_EncryptFinal_ex(ctx, outbuf, &outlen)) { fprintf(stderr, "EncryptFinal failed\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); return 0; }
    if (fwrite(outbuf, 1, outlen, fout) != (size_t)outlen) { perror("fwrite final"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); return 0; }

    EVP_CIPHER_CTX_free(ctx);
    fclose(fin);
    fclose(fout);
    return 1;
}

static int decrypt_file(const char *flac_path, const char *in_path, const char *out_path) {
    unsigned char key[KEY_LEN];
    if (!derive_key_from_file(flac_path, key, KEY_LEN)) {
        fprintf(stderr, "Failed to derive key from FLAC file '%s'\n", flac_path);
        return 0;
    }

    FILE *fin = fopen(in_path, "rb");
    if (!fin) { perror("fopen input"); return 0; }
    FILE *fout = fopen(out_path, "wb");
    if (!fout) { perror("fopen output"); fclose(fin); return 0; }

    unsigned char iv[IV_LEN];
    if (fread(iv, 1, IV_LEN, fin) != IV_LEN) { fprintf(stderr, "Failed to read IV from input\n"); fclose(fin); fclose(fout); return 0; }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { fprintf(stderr, "EVP_CIPHER_CTX_new failed\n"); fclose(fin); fclose(fout); return 0; }

    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) { fprintf(stderr, "DecryptInit failed\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); return 0; }

    unsigned char inbuf[BUF_SIZE];
    unsigned char outbuf[BUF_SIZE + EVP_CIPHER_block_size(EVP_aes_256_cbc())];
    int inlen, outlen;
    while ((inlen = fread(inbuf, 1, BUF_SIZE, fin)) > 0) {
        if (1 != EVP_DecryptUpdate(ctx, outbuf, &outlen, inbuf, inlen)) { fprintf(stderr, "DecryptUpdate failed\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); return 0; }
        if (fwrite(outbuf, 1, outlen, fout) != (size_t)outlen) { perror("fwrite"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); return 0; }
    }
    if (1 != EVP_DecryptFinal_ex(ctx, outbuf, &outlen)) { fprintf(stderr, "DecryptFinal failed: possibly wrong key or corrupted data\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); return 0; }
    if (fwrite(outbuf, 1, outlen, fout) != (size_t)outlen) { perror("fwrite final"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); return 0; }

    EVP_CIPHER_CTX_free(ctx);
    fclose(fin);
    fclose(fout);
    return 1;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  File mode:\n    %s file encrypt <flac-key-file> <input-file> <output-file>\n", prog);
    fprintf(stderr, "    %s file decrypt <flac-key-file> <input-file> <output-file>\n", prog);
    fprintf(stderr, "\n  Spotify mode:\n    %s spotify <encrypt|decrypt> <track-name> <input-file> <output-file>\n", prog);
    fprintf(stderr, "\nSpotify mode requires SPOTIFY_CLIENT_ID and SPOTIFY_CLIENT_SECRET environment variables.\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    const char *mode = argv[1];
    OpenSSL_add_all_algorithms();

    if (strcmp(mode, "file") == 0) {
        if (argc != 6) {
            usage(argv[0]);
            return 2;
        }
        const char *op = argv[2];
        const char *flac = argv[3];
        const char *in = argv[4];
        const char *out = argv[5];

        if (strcmp(op, "encrypt") == 0) {
            if (!encrypt_file(flac, in, out)) return 1;
            printf("Encrypted %s -> %s using key derived from %s\n", in, out, flac);
            return 0;
        } else if (strcmp(op, "decrypt") == 0) {
            if (!decrypt_file(flac, in, out)) return 1;
            printf("Decrypted %s -> %s using key derived from %s\n", in, out, flac);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }

    } else if (strcmp(mode, "spotify") == 0) {
        if (argc != 6) {
            usage(argv[0]);
            return 2;
        }

        const char *op = argv[2];
        const char *track_name = argv[3];
        const char *in = argv[4];
        const char *out = argv[5];

        const char *client_id = getenv("SPOTIFY_CLIENT_ID");
        const char *client_secret = getenv("SPOTIFY_CLIENT_SECRET");

        if (!client_id || !client_secret) {
            fprintf(stderr, "Error: SPOTIFY_CLIENT_ID and SPOTIFY_CLIENT_SECRET environment variables must be set\n");
            return 1;
        }

        SpotifyAuth auth = {0};
        printf("Authenticating with Spotify API...\n");
        if (!spotify_init(client_id, client_secret, &auth)) {
            fprintf(stderr, "Failed to authenticate with Spotify\n");
            return 1;
        }

        printf("Searching for track: %s\n", track_name);
        SpotifyTrack *track = spotify_search_track(&auth, track_name);
        if (!track) {
            fprintf(stderr, "Track not found: %s\n", track_name);
            spotify_auth_free(&auth);
            return 1;
        }

        printf("Downloading preview audio...\n");
        if (!spotify_download_preview(track)) {
            fprintf(stderr, "Failed to download preview audio\n");
            spotify_track_free(track);
            spotify_auth_free(&auth);
            return 1;
        }

        unsigned char key[KEY_LEN];
        if (!spotify_derive_key(track, key, KEY_LEN)) {
            fprintf(stderr, "Failed to derive encryption key\n");
            spotify_track_free(track);
            spotify_auth_free(&auth);
            return 1;
        }

        /* Perform encryption/decryption with Spotify-derived key */
        FILE *fin = fopen(in, "rb");
        if (!fin) { perror("fopen input"); spotify_track_free(track); spotify_auth_free(&auth); return 1; }
        FILE *fout = fopen(out, "wb");
        if (!fout) { perror("fopen output"); fclose(fin); spotify_track_free(track); spotify_auth_free(&auth); return 1; }

        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) { fprintf(stderr, "EVP_CIPHER_CTX_new failed\n"); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }

        unsigned char iv[IV_LEN];

        if (strcmp(op, "encrypt") == 0) {
            if (!RAND_bytes(iv, IV_LEN)) { fprintf(stderr, "Failed to generate IV\n"); fclose(fin); fclose(fout); EVP_CIPHER_CTX_free(ctx); spotify_track_free(track); spotify_auth_free(&auth); return 1; }
            if (fwrite(iv, 1, IV_LEN, fout) != IV_LEN) { perror("fwrite iv"); fclose(fin); fclose(fout); EVP_CIPHER_CTX_free(ctx); spotify_track_free(track); spotify_auth_free(&auth); return 1; }

            if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) { fprintf(stderr, "EncryptInit failed\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }

            unsigned char inbuf[BUF_SIZE];
            unsigned char outbuf[BUF_SIZE + EVP_CIPHER_block_size(EVP_aes_256_cbc())];
            int inlen, outlen;
            while ((inlen = fread(inbuf, 1, BUF_SIZE, fin)) > 0) {
                if (1 != EVP_EncryptUpdate(ctx, outbuf, &outlen, inbuf, inlen)) { fprintf(stderr, "EncryptUpdate failed\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }
                if (fwrite(outbuf, 1, outlen, fout) != (size_t)outlen) { perror("fwrite"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }
            }
            if (1 != EVP_EncryptFinal_ex(ctx, outbuf, &outlen)) { fprintf(stderr, "EncryptFinal failed\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }
            if (fwrite(outbuf, 1, outlen, fout) != (size_t)outlen) { perror("fwrite final"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }

            printf("Encrypted %s -> %s using Spotify track: %s by %s\n", in, out, track->name, track->artist);

        } else if (strcmp(op, "decrypt") == 0) {
            if (fread(iv, 1, IV_LEN, fin) != IV_LEN) { fprintf(stderr, "Failed to read IV from input\n"); fclose(fin); fclose(fout); EVP_CIPHER_CTX_free(ctx); spotify_track_free(track); spotify_auth_free(&auth); return 1; }

            if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) { fprintf(stderr, "DecryptInit failed\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }

            unsigned char inbuf[BUF_SIZE];
            unsigned char outbuf[BUF_SIZE + EVP_CIPHER_block_size(EVP_aes_256_cbc())];
            int inlen, outlen;
            while ((inlen = fread(inbuf, 1, BUF_SIZE, fin)) > 0) {
                if (1 != EVP_DecryptUpdate(ctx, outbuf, &outlen, inbuf, inlen)) { fprintf(stderr, "DecryptUpdate failed\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }
                if (fwrite(outbuf, 1, outlen, fout) != (size_t)outlen) { perror("fwrite"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }
            }
            if (1 != EVP_DecryptFinal_ex(ctx, outbuf, &outlen)) { fprintf(stderr, "DecryptFinal failed: possibly wrong key or corrupted data\n"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }
            if (fwrite(outbuf, 1, outlen, fout) != (size_t)outlen) { perror("fwrite final"); EVP_CIPHER_CTX_free(ctx); fclose(fin); fclose(fout); spotify_track_free(track); spotify_auth_free(&auth); return 1; }

            printf("Decrypted %s -> %s using Spotify track: %s by %s\n", in, out, track->name, track->artist);

        } else {
            usage(argv[0]);
            EVP_CIPHER_CTX_free(ctx);
            fclose(fin);
            fclose(fout);
            spotify_track_free(track);
            spotify_auth_free(&auth);
            return 2;
        }

        EVP_CIPHER_CTX_free(ctx);
        fclose(fin);
        fclose(fout);
        spotify_track_free(track);
        spotify_auth_free(&auth);
        return 0;

    } else {
        usage(argv[0]);
        return 2;
    }
}

# Sonicrypt
Sonicrypt provides a small C program that performs AES-256-CBC encryption and decryption using a key derived from a FLAC file.

Important note about Spotify: Spotify does not provide downloadable FLAC files via its public APIs or streaming service. You must supply a local FLAC file (for example, a track you own or exported from a personal library). This tool derives a 256-bit key by computing the SHA-256 digest of the FLAC file contents.

Build:

```sh
cd Sonicrypt
make
```

Usage:

```sh
# Encrypt: first argument is the FLAC file used as key material
./aes_flac_key encrypt /path/to/key.flac plaintext.bin ciphertext.bin

# Decrypt (reads IV from start of ciphertext):
./aes_flac_key decrypt /path/to/key.flac ciphertext.bin decrypted.bin
```

Notes:
- The program writes a 16-byte IV at the start of the ciphertext file.
- If you want the key to come from a specific Spotify track, obtain a local FLAC file for that track yourself (Spotify streaming content is not suitable).
- This project relies on OpenSSL (`libcrypto`) for cryptographic primitives. Install the development package for your distribution (e.g., `libssl-dev` on Debian/Ubuntu).

Security considerations (short):
- The key is derived directly from the FLAC file by SHA-256; for higher security, consider using a KDF with a salt and iteration count if the FLAC file is predictable.
- Keep FLAC files used as keys private and stored securely.


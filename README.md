# Sonicrypt

Sonicrypt is a C program that performs AES-256-CBC encryption and decryption with keys derived from audio data. It supports two modes:

1. **File Mode**: Derive the key from a local FLAC file
2. **Spotify Mode**: Derive the key from a Spotify track's preview audio combined with track metadata

## Features

- **AES-256-CBC encryption** for secure file encryption
- **Spotify API integration** to search for and download 30-second preview audio
- **Metadata salt enhancement**: Combines preview audio with track metadata (ID, name, artist) to strengthen the key derivation
- **Deterministic key derivation**: The same track always produces the same encryption key
- **IV handling**: Automatically generates and manages initialization vectors

## Installation

### Prerequisites

**Ubuntu/Debian:**
```sh
sudo apt-get install libssl-dev libcurl4-openssl-dev libcjson-dev
```

**macOS (Homebrew):**
```sh
brew install openssl libcurl cjson
```

**Fedora/RHEL:**
```sh
sudo dnf install openssl-devel libcurl-devel cjson-devel
```

### Build

```sh
cd Sonicrypt
make
```

## Usage

### File Mode (Local FLAC File)

Encrypt using a local FLAC file:
```sh
./aes_flac_key file encrypt /path/to/key.flac plaintext.bin ciphertext.bin
```

Decrypt using the same FLAC file:
```sh
./aes_flac_key file decrypt /path/to/key.flac ciphertext.bin decrypted.bin
```

### Spotify Mode (Preview Audio)

#### Step 1: Register a Spotify Application

1. Go to [Spotify Developer Dashboard](https://developer.spotify.com/dashboard)
2. Log in with your Spotify account (create one if needed)
3. Click "Create an App"
4. Accept the terms and create the app
5. Copy your **Client ID** and **Client Secret**

#### Step 2: Set Environment Variables

```sh
export SPOTIFY_CLIENT_ID="your_client_id_here"
export SPOTIFY_CLIENT_SECRET="your_client_secret_here"
```

#### Step 3: Encrypt/Decrypt

Encrypt using a Spotify track:
```sh
./aes_flac_key spotify encrypt "The Beatles - Hey Jude" plaintext.bin ciphertext.bin
```

Decrypt using the same Spotify track:
```sh
./aes_flac_key spotify decrypt "The Beatles - Hey Jude" ciphertext.bin decrypted.bin
```

**Note**: The track search is fuzzy; "Artist - Song Name" format typically works best.

## How It Works

### Key Derivation Process

1. **Download Preview**: Fetches the 30-second preview audio from Spotify
2. **Hash Preview**: SHA-256 hash of the preview audio bytes
3. **Add Metadata Salt**: Combines track ID, name, and artist as salt
4. **Final Key**: SHA-256 hash of (preview_hash || metadata) → 256-bit AES key

This ensures:
- **Deterministic keys**: Same track = same key every time
- **Strong keys**: Preview audio (≈120KB of audio) + metadata provide high entropy
- **Uniqueness**: Different tracks have different keys
- **Security**: Reversing the key requires the preview audio

### Encryption Format

```
[16 bytes: IV] [encrypted data]
```

The IV is randomly generated for each encryption. The same plaintext with the same key will produce different ciphertexts due to the random IV.

## Security Considerations

- **Preview Audio Storage**: Downloaded preview audio is temporarily stored in memory. It's not persisted to disk.
- **Spotify Credentials**: Never commit your Client ID/Secret to version control. Use environment variables.
- **Track Permanence**: If a Spotify track is removed, you cannot decrypt files encrypted with that track without the preview audio.
- **Entropy**: Preview audio provides ~960 kbps × 30s ≈ 3.6 MB of data, which is excellent entropy for key derivation.
- **Keep Credentials Secure**: Store your `SPOTIFY_CLIENT_SECRET` safely, just like API keys.

## Advantages of Spotify Mode

✅ No need to manage local FLAC files  
✅ Easy track sharing (just share the track name/URI)  
✅ Deterministic keys based on public track data  
✅ Spotify tracks won't change (preview audio stays the same for a given track ID)  
✅ Works offline once preview is downloaded  

## Limitations

- Only 30-second preview audio is available (not full tracks)
- Requires active internet for initial setup and track search
- Spotify rate limits apply to search requests
- Some tracks may not have preview audio available

## Examples

### Encrypt a document with a Spotify track
```sh
export SPOTIFY_CLIENT_ID="your_id"
export SPOTIFY_CLIENT_SECRET="your_secret"

./aes_flac_key spotify encrypt "Pink Floyd - Wish You Were Here" secret.txt encrypted.bin
```

### Decrypt it later
```sh
./aes_flac_key spotify decrypt "Pink Floyd - Wish You Were Here" encrypted.bin secret.txt
```

### Encrypt with a local FLAC file
```sh
./aes_flac_key file encrypt ~/Music/favorite-song.flac important.pdf locked.bin
./aes_flac_key file decrypt ~/Music/favorite-song.flac locked.bin important.pdf
```

## Technical Details

- **Cipher**: AES-256-CBC (Advanced Encryption Standard)
- **Key Size**: 256 bits (32 bytes)
- **IV Size**: 128 bits (16 bytes, random per encryption)
- **KDF**: SHA-256 (from OpenSSL)
- **Library**: OpenSSL (libcrypto), libcurl, cJSON

## Troubleshooting

**"Failed to authenticate with Spotify"**
- Check that `SPOTIFY_CLIENT_ID` and `SPOTIFY_CLIENT_SECRET` are set correctly
- Verify your Spotify app credentials in the Developer Dashboard

**"Track not found"**
- Try a different search term (e.g., "Artist - Title" or just the song name)
- Ensure the track exists on Spotify
- Some regional tracks may not be available in all markets

**"No preview available"**
- Not all Spotify tracks have preview audio
- Try searching for a more popular track

**Compilation errors**
- Ensure all dependencies are installed: `libssl-dev`, `libcurl4-openssl-dev`, `libcjson-dev`
- On macOS with Homebrew, you may need to specify paths: `LDFLAGS="-L/usr/local/opt/openssl/lib" make`

## License

MIT License (or specify your preferred license)



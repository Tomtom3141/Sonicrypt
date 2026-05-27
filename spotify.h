#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <stddef.h>

typedef struct {
    char *id;
    char *name;
    char *artist;
    char *preview_url;
    unsigned char *preview_data;
    size_t preview_size;
} SpotifyTrack;

typedef struct {
    char *access_token;
    long expires_at;
} SpotifyAuth;

/* Initialize Spotify authentication. Returns 0 on failure. */
int spotify_init(const char *client_id, const char *client_secret, SpotifyAuth *auth);

/* Search for a track on Spotify. Returns track info or NULL on failure. */
SpotifyTrack* spotify_search_track(SpotifyAuth *auth, const char *query);

/* Download preview audio for a track. Returns 0 on failure. */
int spotify_download_preview(SpotifyTrack *track);

/* Derive encryption key from preview audio + metadata. */
int spotify_derive_key(SpotifyTrack *track, unsigned char *key, size_t key_len);

/* Free SpotifyTrack resources. */
void spotify_track_free(SpotifyTrack *track);

/* Free SpotifyAuth resources. */
void spotify_auth_free(SpotifyAuth *auth);

#endif

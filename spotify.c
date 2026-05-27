#include "spotify.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <openssl/sha.h>

#define SPOTIFY_AUTH_URL "https://accounts.spotify.com/api/token"
#define SPOTIFY_API_URL "https://api.spotify.com/v1"
#define BUFFER_SIZE 4096

typedef struct {
    char *data;
    size_t size;
} ResponseBuffer;

static size_t curl_response_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    ResponseBuffer *buf = (ResponseBuffer *)userp;
    char *ptr = realloc(buf->data, buf->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Not enough memory for curl response\n");
        return 0;
    }
    buf->data = ptr;
    memcpy(&(buf->data[buf->size]), contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = 0;
    return realsize;
}

int spotify_init(const char *client_id, const char *client_secret, SpotifyAuth *auth) {
    if (!client_id || !client_secret) {
        fprintf(stderr, "Spotify credentials required\n");
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    char credentials[512];
    snprintf(credentials, sizeof(credentials), "%s:%s", client_id, client_secret);

    ResponseBuffer response = {0};
    curl_easy_setopt(curl, CURLOPT_URL, SPOTIFY_AUTH_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD, credentials);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "grant_type=client_credentials");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "Spotify auth request failed: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }

    cJSON *json = cJSON_Parse(response.data);
    free(response.data);
    if (!json) {
        fprintf(stderr, "Failed to parse Spotify auth response\n");
        return 0;
    }

    cJSON *token_item = cJSON_GetObjectItemCaseSensitive(json, "access_token");
    cJSON *expires_item = cJSON_GetObjectItemCaseSensitive(json, "expires_in");

    if (!token_item || !expires_item) {
        fprintf(stderr, "Invalid Spotify auth response\n");
        cJSON_Delete(json);
        return 0;
    }

    auth->access_token = malloc(strlen(token_item->valuestring) + 1);
    strcpy(auth->access_token, token_item->valuestring);
    auth->expires_at = time(NULL) + expires_item->valueint;

    cJSON_Delete(json);
    return 1;
}

SpotifyTrack* spotify_search_track(SpotifyAuth *auth, const char *query) {
    if (!auth->access_token) {
        fprintf(stderr, "Not authenticated with Spotify\n");
        return NULL;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    char *encoded_query = curl_easy_escape(curl, query, 0);
    char url[1024];
    snprintf(url, sizeof(url), "%s/search?q=%s&type=track&limit=1", SPOTIFY_API_URL, encoded_query);
    curl_free(encoded_query);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", auth->access_token);
    struct curl_slist *headers = curl_slist_append(NULL, auth_header);

    ResponseBuffer response = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "Spotify search failed: %s\n", curl_easy_strerror(res));
        free(response.data);
        return NULL;
    }

    cJSON *json = cJSON_Parse(response.data);
    free(response.data);
    if (!json) {
        fprintf(stderr, "Failed to parse Spotify search response\n");
        return NULL;
    }

    cJSON *tracks = cJSON_GetObjectItemCaseSensitive(json, "tracks");
    cJSON *items = cJSON_GetObjectItemCaseSensitive(tracks, "items");
    cJSON *first_track = cJSON_GetArrayItem(items, 0);

    if (!first_track) {
        fprintf(stderr, "No tracks found matching query: %s\n", query);
        cJSON_Delete(json);
        return NULL;
    }

    SpotifyTrack *track = malloc(sizeof(SpotifyTrack));
    memset(track, 0, sizeof(SpotifyTrack));

    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(first_track, "id");
    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(first_track, "name");
    cJSON *artists_item = cJSON_GetObjectItemCaseSensitive(first_track, "artists");
    cJSON *preview_item = cJSON_GetObjectItemCaseSensitive(first_track, "preview_url");

    if (id_item) {
        track->id = malloc(strlen(id_item->valuestring) + 1);
        strcpy(track->id, id_item->valuestring);
    }
    if (name_item) {
        track->name = malloc(strlen(name_item->valuestring) + 1);
        strcpy(track->name, name_item->valuestring);
    }
    if (artists_item && artists_item->child) {
        cJSON *artist_obj = cJSON_GetArrayItem(artists_item, 0);
        cJSON *artist_name = cJSON_GetObjectItemCaseSensitive(artist_obj, "name");
        if (artist_name) {
            track->artist = malloc(strlen(artist_name->valuestring) + 1);
            strcpy(track->artist, artist_name->valuestring);
        }
    }
    if (preview_item && preview_item->valuestring) {
        track->preview_url = malloc(strlen(preview_item->valuestring) + 1);
        strcpy(track->preview_url, preview_item->valuestring);
    }

    cJSON_Delete(json);
    return track;
}

int spotify_download_preview(SpotifyTrack *track) {
    if (!track->preview_url) {
        fprintf(stderr, "Track has no preview URL available\n");
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    ResponseBuffer response = {0};
    response.data = malloc(1);

    curl_easy_setopt(curl, CURLOPT_URL, track->preview_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "Failed to download preview: %s\n", curl_easy_strerror(res));
        free(response.data);
        return 0;
    }

    track->preview_data = (unsigned char *)response.data;
    track->preview_size = response.size;

    printf("Downloaded %zu bytes of preview audio\n", track->preview_size);
    return 1;
}

int spotify_derive_key(SpotifyTrack *track, unsigned char *key, size_t key_len) {
    if (!track->preview_data || track->preview_size == 0) {
        fprintf(stderr, "No preview data available for key derivation\n");
        return 0;
    }

    /* Create metadata salt */
    char metadata_str[512];
    snprintf(metadata_str, sizeof(metadata_str), "%s|%s|%s",
             track->id ? track->id : "unknown",
             track->name ? track->name : "unknown",
             track->artist ? track->artist : "unknown");

    /* Hash preview data */
    SHA256_CTX sha;
    unsigned char intermediate[SHA256_DIGEST_LENGTH];
    SHA256_Init(&sha);
    SHA256_Update(&sha, track->preview_data, track->preview_size);
    SHA256_Final(intermediate, &sha);

    /* Hash intermediate + metadata for final key (metadata acts as salt) */
    SHA256_Init(&sha);
    SHA256_Update(&sha, intermediate, SHA256_DIGEST_LENGTH);
    SHA256_Update(&sha, (unsigned char *)metadata_str, strlen(metadata_str));
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &sha);

    if (key_len > SHA256_DIGEST_LENGTH) return 0;
    memcpy(key, digest, key_len);

    printf("Derived encryption key from: \"%s\" by %s (ID: %s)\n",
           track->name ? track->name : "unknown",
           track->artist ? track->artist : "unknown",
           track->id ? track->id : "unknown");

    return 1;
}

void spotify_track_free(SpotifyTrack *track) {
    if (!track) return;
    free(track->id);
    free(track->name);
    free(track->artist);
    free(track->preview_url);
    free(track->preview_data);
    free(track);
}

void spotify_auth_free(SpotifyAuth *auth) {
    if (!auth) return;
    free(auth->access_token);
    auth->access_token = NULL;
}

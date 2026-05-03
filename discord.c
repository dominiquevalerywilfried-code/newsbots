/* discord.c - Envoie des messages sur Discord et écoute "!go"
 *
 * Deux mécanismes :
 *   1. Webhook Discord  → pour ENVOYER des messages (simple POST)
 *   2. Discord REST API → pour LIRE les messages (polling toutes les 5s)
 *
 * Pas de WebSocket : on poll le dernier message du channel.
 * C'est suffisant pour un usage personnel.
 *
 * Variables d'environnement requises :
 *   DISCORD_TOKEN       : le token de ton bot Discord
 *   DISCORD_CHANNEL_ID  : l'ID du channel où tu écris "!go"
 *   DISCORD_WEBHOOK_URL : (optionnel) URL du webhook pour envoyer
 */

#include "newsbot.h"
#include <curl/curl.h>

#define DISCORD_API       "https://discord.com/api/v10"
#define MAX_MSG_LEN       1900  /* Discord limite à 2000 chars */
#define POLL_INTERVAL_SEC 5

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    HttpResponse *r = (HttpResponse *)userdata;
    size_t total    = size * nmemb;
    char *tmp = realloc(r->data, r->size + total + 1);
    if (!tmp) return 0;
    r->data = tmp;
    memcpy(r->data + r->size, ptr, total);
    r->size         += total;
    r->data[r->size] = '\0';
    return total;
}

/* ─── Envoie un message sur Discord ─────────────────────────
 *
 * Utilise le token bot + channel ID.
 * Si le message dépasse 2000 chars, on le découpe.
 */
int discord_send(const char *message)
{
    const char *token      = getenv(DISCORD_TOKEN_ENV);
    const char *channel_id = getenv(DISCORD_CHANNEL_ENV);

    if (!token || !channel_id) {
        log_error("DISCORD_TOKEN ou DISCORD_CHANNEL_ID non défini");
        printf("\n=== RÉSUMÉ (Discord non configuré) ===\n%s\n", message);
        return -1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    /* Découpe le message si > MAX_MSG_LEN */
    size_t total_len = strlen(message);
    size_t offset    = 0;
    int    part      = 1;

    while (offset < total_len) {
        size_t chunk_len = total_len - offset;
        if (chunk_len > MAX_MSG_LEN) chunk_len = MAX_MSG_LEN;

        /* Ne coupe pas au milieu d'un mot */
        if (offset + chunk_len < total_len) {
            while (chunk_len > 0 && message[offset + chunk_len] != '\n' &&
                   message[offset + chunk_len] != ' ')
                chunk_len--;
            if (chunk_len == 0) chunk_len = MAX_MSG_LEN;
        }

        /* Construit le chunk */
        char chunk[MAX_MSG_LEN + 1];
        memcpy(chunk, message + offset, chunk_len);
        chunk[chunk_len] = '\0';

        /* Échappe pour JSON */
        char escaped[MAX_MSG_LEN * 2 + 1];
        size_t ej = 0;
        for (size_t i = 0; chunk[i] && ej < sizeof(escaped) - 2; i++) {
            if      (chunk[i] == '"')  { escaped[ej++] = '\\'; escaped[ej++] = '"'; }
            else if (chunk[i] == '\\') { escaped[ej++] = '\\'; escaped[ej++] = '\\'; }
            else if (chunk[i] == '\n') { escaped[ej++] = '\\'; escaped[ej++] = 'n'; }
            else                       { escaped[ej++] = chunk[i]; }
        }
        escaped[ej] = '\0';

        char json[MAX_MSG_LEN * 2 + 64];
        snprintf(json, sizeof(json), "{\"content\":\"%s\"}", escaped);

        /* URL de l'endpoint */
        char url[256];
        snprintf(url, sizeof(url), "%s/channels/%s/messages",
                 DISCORD_API, channel_id);

        char auth[256];
        snprintf(auth, sizeof(auth), "Authorization: Bot %s", token);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, auth);

        HttpResponse r = {0};

        curl_easy_setopt(curl, CURLOPT_URL,            url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     json);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &r);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,        15L);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            log_error("Discord send error: %s", curl_easy_strerror(res));
        else
            log_info("Message Discord envoyé (partie %d, %zu chars)", part, chunk_len);

        curl_slist_free_all(headers);
        if (r.data) free(r.data);

        offset += chunk_len;
        part++;

        /* Petit délai entre les parties (rate limit Discord) */
        if (offset < total_len) sleep(1);
    }

    curl_easy_cleanup(curl);
    return 0;
}

/* ─── Extrait l'ID du dernier message vu ─── */
static char last_message_id[64] = "";

/* ─── Récupère les derniers messages du channel ─────────────
 *
 * Retourne 1 si "!go" ou "!résumé" ou "!resume" est trouvé.
 * Retourne 0 sinon.
 */
int discord_listen_for_go(void)
{
    const char *token      = getenv(DISCORD_TOKEN_ENV);
    const char *channel_id = getenv(DISCORD_CHANNEL_ENV);

    if (!token || !channel_id) return 0;

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    char url[256];
    if (last_message_id[0])
        snprintf(url, sizeof(url),
                 "%s/channels/%s/messages?limit=5&after=%s",
                 DISCORD_API, channel_id, last_message_id);
    else
        snprintf(url, sizeof(url),
                 "%s/channels/%s/messages?limit=1",
                 DISCORD_API, channel_id);

    char auth[256];
    snprintf(auth, sizeof(auth), "Authorization: Bot %s", token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth);

    HttpResponse r = {0};

    curl_easy_setopt(curl, CURLOPT_URL,            url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &r);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !r.data) {
        if (r.data) free(r.data);
        return 0;
    }

    int found_go = 0;

    /* Parse JSON minimaliste : cherche "content":"!go" */
    const char *p = r.data;
    while ((p = strstr(p, "\"content\":\"")) != NULL) {
        p += 11; /* après "content":" */

        /* Extrait le contenu */
        char content[256] = "";
        size_t i = 0;
        while (*p && *p != '"' && i < sizeof(content) - 1) {
            if (*p == '\\') p++; /* skip escape */
            content[i++] = *p++;
        }
        content[i] = '\0';

        /* Commandes reconnues */
        if (strcasecmp(content, "!go")     == 0 ||
            strcasecmp(content, "!resume") == 0 ||
            strcasecmp(content, "!résumé") == 0 ||
            strcasecmp(content, "go")      == 0)
        {
            found_go = 1;
        }
    }

    /* Met à jour le dernier ID vu pour ne pas relire les mêmes messages */
    const char *id_marker = "\"id\":\"";
    const char *id_p = strstr(r.data, id_marker);
    if (id_p) {
        id_p += strlen(id_marker);
        size_t i = 0;
        char new_id[64] = "";
        while (*id_p && *id_p != '"' && i < sizeof(new_id) - 1)
            new_id[i++] = *id_p++;
        new_id[i] = '\0';
        if (new_id[0]) strncpy(last_message_id, new_id, sizeof(last_message_id) - 1);
    }

    free(r.data);
    return found_go;
}

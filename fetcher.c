/* fetcher.c - Télécharge les pages et flux RSS via libcurl
 *
 * Gère :
 *   - ETag / If-None-Match
 *   - Last-Modified / If-Modified-Since
 *   - User-Agent neutre
 *   - Réponse 304 = pas de changement
 */

#include "newsbot.h"
#include <curl/curl.h>

/* Callback interne libcurl : accumule les données reçues */
static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    HttpResponse *r = (HttpResponse *)userdata;
    size_t total    = size * nmemb;

    char *tmp = realloc(r->data, r->size + total + 1);
    if (!tmp) return 0;

    r->data = tmp;
    memcpy(r->data + r->size, ptr, total);
    r->size          += total;
    r->data[r->size]  = '\0';
    return total;
}

/* Callback pour les headers HTTP (ETag, Last-Modified) */
static size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata)
{
    HttpResponse *r = (HttpResponse *)userdata;
    size_t len = size * nitems;
    char line[512];

    if (len >= sizeof(line)) return len;
    memcpy(line, buffer, len);
    line[len] = '\0';

    /* Retire le \r\n final */
    char *end = line + strlen(line) - 1;
    while (end > line && (*end == '\r' || *end == '\n')) *end-- = '\0';

    if (strncasecmp(line, "ETag:", 5) == 0) {
        strncpy(r->etag, line + 6, sizeof(r->etag) - 1);
    } else if (strncasecmp(line, "Last-Modified:", 14) == 0) {
        strncpy(r->last_modified, line + 15, sizeof(r->last_modified) - 1);
    }

    return len;
}

/* ─── Fonction principale : fetch une URL ───────────────────
 *
 * etag     : l'ETag connu (ou NULL)
 * last_mod : le Last-Modified connu (ou NULL)
 *
 * Retourne NULL si erreur réseau.
 * Si http_code == 304 → rien de nouveau, data == NULL.
 */
HttpResponse *http_fetch(const char *url,
                         const char *etag,
                         const char *last_mod)
{
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    HttpResponse *r = calloc(1, sizeof(HttpResponse));
    if (!r) { curl_easy_cleanup(curl); return NULL; }

    /* Headers conditionnels */
    struct curl_slist *headers = NULL;
    char hdr[600];

    if (etag && etag[0]) {
        snprintf(hdr, sizeof(hdr), "If-None-Match: %s", etag);
        headers = curl_slist_append(headers, hdr);
    }
    if (last_mod && last_mod[0]) {
        snprintf(hdr, sizeof(hdr), "If-Modified-Since: %s", last_mod);
        headers = curl_slist_append(headers, hdr);
    }

    headers = curl_slist_append(headers,
        "User-Agent: NewsBot/1.0 (RSS Reader; +https://github.com/toi/newsbot)");
    headers = curl_slist_append(headers, "Accept: application/rss+xml, application/xml, text/html, */*");

    curl_easy_setopt(curl, CURLOPT_URL,            url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      r);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA,     r);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS,      5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        log_error("curl error [%s]: %s", url, curl_easy_strerror(res));
        http_response_free(r);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return NULL;
    }

    long code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    r->http_code = (int)code;

    if (code == 304) {
        /* Pas de nouveau contenu */
        free(r->data);
        r->data = NULL;
        r->size = 0;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return r;
}

void http_response_free(HttpResponse *r)
{
    if (!r) return;
    if (r->data) free(r->data);
    free(r);
}

/* summarizer.c - Envoie les articles à l'API Groq pour résumé en français
 *
 * Groq est gratuit (14 400 requêtes/jour) et héberge Mixtral/LLaMA.
 * On envoie une requête HTTP POST avec les titres + descriptions.
 * Le modèle résume TOUT en français, peu importe la langue source.
 */

#include "newsbot.h"
#include <curl/curl.h>

#define GROQ_API_URL  "https://api.groq.com/openai/v1/chat/completions"
#define GROQ_MODEL    "mixtral-8x7b-32768"
#define MAX_PROMPT    16000

/* Callback libcurl pour accumuler la réponse */
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

/* Extrait le champ "content" de la réponse JSON Groq (sans lib JSON) */
static int extract_content(const char *json, char *out, size_t out_size)
{
    /* On cherche : "content":"..." dans la réponse */
    const char *marker = "\"content\":\"";
    const char *p = strstr(json, marker);
    if (!p) return 0;
    p += strlen(marker);

    size_t j = 0;
    while (*p && j < out_size - 1) {
        if (*p == '\\' && *(p+1) == '"') {
            out[j++] = '"';
            p += 2;
        } else if (*p == '\\' && *(p+1) == 'n') {
            out[j++] = '\n';
            p += 2;
        } else if (*p == '"') {
            break; /* fin du champ */
        } else {
            out[j++] = *p++;
        }
    }
    out[j] = '\0';
    return j > 0;
}

/* ─── Fonction principale ────────────────────────────────────
 *
 * Prend un tableau d'articles, construit un prompt,
 * appelle Groq, retourne le résumé en français dans `out`.
 */
int summarize_articles(Article *articles, int count,
                       char *out, size_t out_size)
{
    const char *api_key = getenv(GROQ_API_KEY_ENV);
    if (!api_key || !api_key[0]) {
        log_error("Variable GROQ_API_KEY non définie !");
        strncpy(out,
            "❌ Erreur : variable GROQ_API_KEY manquante. "
            "Définis-la avec : export GROQ_API_KEY=ta_clé",
            out_size - 1);
        return -1;
    }

    if (count == 0) {
        strncpy(out, "Aucun nouvel article depuis la dernière vérification.", out_size - 1);
        return 0;
    }

    /* ── Construction du prompt ── */
    char prompt[MAX_PROMPT];
    int  offset = 0;

    offset += snprintf(prompt + offset, sizeof(prompt) - offset,
        "Tu es un assistant qui résume l'actualité. "
        "Voici %d articles récents en différentes langues. "
        "Résume chacun EN FRANÇAIS en 2-3 phrases maximum. "
        "Groupe-les par thème (Espace, Tech, IA, Économie, Jeux vidéo...). "
        "Sois concis et informatif. "
        "Pour chaque article, indique la source entre crochets.\n\n"
        "ARTICLES :\n", count);

    for (int i = 0; i < count && offset < MAX_PROMPT - 200; i++) {
        offset += snprintf(prompt + offset, sizeof(prompt) - offset,
            "---\nTITRE : %s\nSOURCE : %s\nLIEN : %s\nDESCRIPTION : %.300s\n",
            articles[i].title,
            articles[i].source_name,
            articles[i].url,
            articles[i].description);
    }

    /* ── Construction du JSON pour l'API ── */
    /* Échappe les guillemets et sauts de ligne dans le prompt */
    char escaped[MAX_PROMPT * 2];
    size_t ej = 0;
    for (size_t i = 0; prompt[i] && ej < sizeof(escaped) - 2; i++) {
        if      (prompt[i] == '"')  { escaped[ej++] = '\\'; escaped[ej++] = '"'; }
        else if (prompt[i] == '\n') { escaped[ej++] = '\\'; escaped[ej++] = 'n'; }
        else if (prompt[i] == '\\') { escaped[ej++] = '\\'; escaped[ej++] = '\\'; }
        else                        { escaped[ej++] = prompt[i]; }
    }
    escaped[ej] = '\0';

    char json_body[MAX_PROMPT * 2 + 256];
    snprintf(json_body, sizeof(json_body),
        "{\"model\":\"%s\","
        "\"max_tokens\":1500,"
        "\"messages\":["
        "{\"role\":\"user\",\"content\":\"%s\"}"
        "]}",
        GROQ_MODEL, escaped);

    /* ── Requête HTTP ── */
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    HttpResponse r = {0};

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL,            GROQ_API_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     json_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &r);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        log_error("Groq API erreur curl: %s", curl_easy_strerror(res));
        if (r.data) free(r.data);
        return -1;
    }

    if (!r.data) return -1;

    int ok = extract_content(r.data, out, out_size);
    free(r.data);

    if (!ok) {
        log_error("Impossible d'extraire la réponse Groq. Réponse brute: %.200s",
                  r.data ? r.data : "(null)");
        strncpy(out, "❌ Erreur lors de la lecture de la réponse Groq.", out_size - 1);
        return -1;
    }

    return 0;
}

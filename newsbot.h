#ifndef NEWSBOT_H
#define NEWSBOT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════
   NEWSBOT - Bot d'actualités personnalisé
   Auteur : généré pour toi
   Langues supportées : FR, EN, DE
   Résumés : toujours en français
   ═══════════════════════════════════════════════════════════ */

#define MAX_URL_LEN       512
#define MAX_TITLE_LEN     256
#define MAX_DESC_LEN      2048
#define MAX_HASH_LEN      65
#define MAX_SOURCES       64
#define MAX_ARTICLES      512
#define MAX_QUEUE         1024
#define GROQ_API_KEY_ENV  "GROQ_API_KEY"
#define DISCORD_TOKEN_ENV "DISCORD_TOKEN"
#define DISCORD_CHANNEL_ENV "DISCORD_CHANNEL_ID"
#define DB_FILE           "newsbot.db"
#define SEEN_FILE         "seen_urls.txt"
#define LOG_FILE          "newsbot.log"

/* Fréquences de vérification en secondes */
#define FREQ_HIGH    1800   /* 30 min  - SpaceX launches, actus chaudes */
#define FREQ_MEDIUM  7200   /* 2h      - YouTube, blogs tech */
#define FREQ_LOW     86400  /* 24h     - sites peu actifs */

/* ─── Structure : une source RSS ou page web ─── */
typedef struct {
    char  url[MAX_URL_LEN];
    char  name[128];
    char  last_etag[128];
    char  last_modified[64];
    char  content_hash[MAX_HASH_LEN];
    time_t last_checked;
    int   check_interval;   /* en secondes */
    int   is_rss;           /* 1=RSS/Atom, 0=HTML */
    int   enabled;
} Source;

/* ─── Structure : un article extrait ─── */
typedef struct {
    char  title[MAX_TITLE_LEN];
    char  url[MAX_URL_LEN];
    char  description[MAX_DESC_LEN];
    char  source_name[128];
    time_t published;
    int   sent;             /* 1 si déjà envoyé sur Discord */
} Article;

/* ─── File d'attente pour le crawling ─── */
typedef struct {
    char urls[MAX_QUEUE][MAX_URL_LEN];
    int  head;
    int  tail;
    int  count;
} UrlQueue;

/* ─── Réponse HTTP brute ─── */
typedef struct {
    char  *data;
    size_t size;
    char  etag[128];
    char  last_modified[64];
    int   http_code;
} HttpResponse;

/* Prototypes des modules */

/* fetcher.c */
HttpResponse* http_fetch(const char *url, const char *etag, const char *last_mod);
void          http_response_free(HttpResponse *r);

/* parser.c */
int  parse_rss(const char *xml, const char *source_name, Article *out, int max);
int  parse_html_links(const char *html, const char *base_url,
                      char out[][MAX_URL_LEN], int max);
void extract_text_from_html(const char *html, char *out, size_t out_size);

/* storage.c */
int  storage_init(void);
int  storage_save_source(const Source *s);
int  storage_load_sources(Source *out, int max);
int  storage_save_article(const Article *a);
int  storage_load_unsent(Article *out, int max);
int  storage_mark_sent(const char *url);
int  storage_url_seen(const char *url);
int  storage_add_seen(const char *url);

/* summarizer.c */
int  summarize_articles(Article *articles, int count, char *out, size_t out_size);

/* discord.c */
int  discord_send(const char *message);
int  discord_listen_for_go(void);  /* retourne 1 si "!go" reçu */

/* hash.c */
void sha256_string(const char *input, size_t len, char out_hex[65]);

/* log.c */
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif /* NEWSBOT_H */

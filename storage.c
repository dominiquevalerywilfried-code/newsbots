/* storage.c - Stockage des articles et sources vus
 *
 * On utilise des fichiers texte simples pour éviter SQLite
 * (zéro dépendance externe supplémentaire).
 *
 * Format seen_urls.txt : une URL par ligne
 * Format articles.csv  : titre|url|source|timestamp|sent
 */

#include "newsbot.h"
#include <errno.h>

#define ARTICLES_FILE  "articles.csv"
#define SOURCES_FILE   "sources.cfg"
#define SEP            "|"

/* ─── Initialisation ─── */
int storage_init(void)
{
    /* Crée les fichiers s'ils n'existent pas */
    FILE *f;

    f = fopen(SEEN_FILE, "a");
    if (!f) { log_error("Cannot create %s: %s", SEEN_FILE, strerror(errno)); return -1; }
    fclose(f);

    f = fopen(ARTICLES_FILE, "a");
    if (!f) { log_error("Cannot create %s", ARTICLES_FILE); return -1; }
    fclose(f);

    log_info("Storage initialisé.");
    return 0;
}

/* ─── Vérifie si une URL a déjà été vue ─── */
int storage_url_seen(const char *url)
{
    FILE *f = fopen(SEEN_FILE, "r");
    if (!f) return 0;

    char line[MAX_URL_LEN];
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Retire \n */
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, url) == 0) { found = 1; break; }
    }

    fclose(f);
    return found;
}

/* ─── Marque une URL comme vue ─── */
int storage_add_seen(const char *url)
{
    if (storage_url_seen(url)) return 0; /* déjà présente */

    FILE *f = fopen(SEEN_FILE, "a");
    if (!f) return -1;
    fprintf(f, "%s\n", url);
    fclose(f);
    return 0;
}

/* ─── Sauvegarde un article ─── */
int storage_save_article(const Article *a)
{
    if (!a || !a->url[0]) return -1;

    /* Vérifie si déjà présent */
    if (storage_url_seen(a->url)) return 0;

    FILE *f = fopen(ARTICLES_FILE, "a");
    if (!f) return -1;

    /* Titre sans pipe pour ne pas casser le CSV */
    char safe_title[MAX_TITLE_LEN];
    char safe_desc[512];

    strncpy(safe_title, a->title, sizeof(safe_title) - 1);
    strncpy(safe_desc,  a->description, sizeof(safe_desc) - 1);

    /* Remplace les | par des espaces */
    for (char *p = safe_title; *p; p++) if (*p == '|') *p = '-';
    for (char *p = safe_desc;  *p; p++) if (*p == '|') *p = '-';
    /* Retire les sauts de ligne */
    for (char *p = safe_title; *p; p++) if (*p == '\n' || *p == '\r') *p = ' ';
    for (char *p = safe_desc;  *p; p++) if (*p == '\n' || *p == '\r') *p = ' ';

    fprintf(f, "%s|%s|%s|%ld|0\n",
            safe_title,
            a->url,
            a->source_name,
            (long)a->published);

    fclose(f);
    storage_add_seen(a->url);
    return 0;
}

/* ─── Charge les articles non encore envoyés ─── */
int storage_load_unsent(Article *out, int max)
{
    FILE *f = fopen(ARTICLES_FILE, "r");
    if (!f) return 0;

    int count = 0;
    char line[MAX_URL_LEN + MAX_TITLE_LEN + 256];

    while (fgets(line, sizeof(line), f) && count < max) {
        line[strcspn(line, "\n")] = '\0';
        if (!line[0]) continue;

        /* Parse : titre|url|source|timestamp|sent */
        char *title  = strtok(line, SEP);
        char *url    = strtok(NULL, SEP);
        char *source = strtok(NULL, SEP);
        char *ts_s   = strtok(NULL, SEP);
        char *sent_s = strtok(NULL, SEP);

        if (!title || !url || !source || !sent_s) continue;

        int sent = atoi(sent_s);
        if (sent) continue; /* déjà envoyé */

        Article *a = &out[count];
        memset(a, 0, sizeof(Article));
        strncpy(a->title,       title,  sizeof(a->title) - 1);
        strncpy(a->url,         url,    sizeof(a->url) - 1);
        strncpy(a->source_name, source, sizeof(a->source_name) - 1);
        a->published = ts_s ? (time_t)atol(ts_s) : 0;
        a->sent = 0;
        count++;
    }

    fclose(f);
    return count;
}

/* ─── Marque un article comme envoyé ─── */
int storage_mark_sent(const char *url)
{
    /* Charge tout le fichier, modifie la ligne, réécrit */
    FILE *f = fopen(ARTICLES_FILE, "r");
    if (!f) return -1;

    /* Lit tout le contenu */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(fsize + 1);
    if (!content) { fclose(f); return -1; }
    fread(content, 1, fsize, f);
    content[fsize] = '\0';
    fclose(f);

    /* Cherche la ligne contenant l'URL et change le |0\n en |1\n */
    char *p = content;
    while ((p = strstr(p, url)) != NULL) {
        /* Vérifie que c'est bien le champ URL (précédé d'un |) */
        if (p > content && *(p-1) == '|') {
            /* Cherche le |0 en fin de ligne */
            char *eol = strchr(p, '\n');
            if (eol && eol > p + 2 && *(eol-1) == '0' && *(eol-2) == '|') {
                *(eol-1) = '1';
                break;
            }
        }
        p++;
    }

    /* Réécrit */
    f = fopen(ARTICLES_FILE, "w");
    if (!f) { free(content); return -1; }
    fwrite(content, 1, fsize, f);
    fclose(f);
    free(content);
    return 0;
}

/* ─── Sauvegarde l'état d'une source (ETag, dates) ─── */
int storage_save_source(const Source *s)
{
    /* Sources stockées dans sources.cfg : une par ligne
     * format : url|etag|last_modified|content_hash|last_checked */

    /* Charge tout, met à jour ou ajoute, réécrit */
    FILE *f = fopen(SOURCES_FILE, "r");
    char *content = NULL;
    long fsize = 0;

    if (f) {
        fseek(f, 0, SEEK_END);
        fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        content = malloc(fsize + 512);
        if (content) fread(content, 1, fsize, f);
        fclose(f);
    }

    if (!content) {
        content = malloc(512);
        if (!content) return -1;
        fsize = 0;
        content[0] = '\0';
    }

    /* Cherche et remplace la ligne existante */
    char new_line[MAX_URL_LEN + 256];
    snprintf(new_line, sizeof(new_line), "%s|%s|%s|%s|%ld\n",
             s->url, s->last_etag, s->last_modified,
             s->content_hash, (long)s->last_checked);

    char *found = strstr(content, s->url);
    if (found) {
        char *eol = strchr(found, '\n');
        if (eol) {
            size_t before = (size_t)(found - content);
            size_t after_offset = (size_t)(eol - content) + 1;
            size_t new_len = before + strlen(new_line) + (fsize - after_offset) + 1;
            char *new_content = malloc(new_len);
            if (!new_content) { free(content); return -1; }
            memcpy(new_content, content, before);
            strcpy(new_content + before, new_line);
            strcat(new_content, content + after_offset);
            free(content);
            content = new_content;
            fsize = (long)strlen(content);
        }
    } else {
        /* Ajoute en fin */
        content = realloc(content, fsize + strlen(new_line) + 1);
        if (!content) return -1;
        strcat(content, new_line);
        fsize = (long)strlen(content);
    }

    f = fopen(SOURCES_FILE, "w");
    if (!f) { free(content); return -1; }
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    free(content);
    return 0;
}

/* ─── Charge l'état des sources depuis le fichier ─── */
int storage_load_sources(Source *out, int max)
{
    FILE *f = fopen(SOURCES_FILE, "r");
    if (!f) return 0;

    int count = 0;
    char line[MAX_URL_LEN + 256];

    while (fgets(line, sizeof(line), f) && count < max) {
        line[strcspn(line, "\n")] = '\0';
        if (!line[0] || line[0] == '#') continue;

        char *url      = strtok(line, SEP);
        char *etag     = strtok(NULL, SEP);
        char *last_mod = strtok(NULL, SEP);
        char *hash     = strtok(NULL, SEP);
        char *ts_s     = strtok(NULL, SEP);

        if (!url) continue;

        Source *src = &out[count];
        strncpy(src->url,           url,      sizeof(src->url) - 1);
        if (etag)     strncpy(src->last_etag,      etag,     sizeof(src->last_etag) - 1);
        if (last_mod) strncpy(src->last_modified,  last_mod, sizeof(src->last_modified) - 1);
        if (hash)     strncpy(src->content_hash,   hash,     sizeof(src->content_hash) - 1);
        src->last_checked = ts_s ? (time_t)atol(ts_s) : 0;
        count++;
    }

    fclose(f);
    return count;
}

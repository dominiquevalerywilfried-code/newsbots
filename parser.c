/* parser.c - Parse le XML RSS/Atom et extrait les articles
 *            Parse aussi le HTML pour extraire les liens
 *
 * Pas de dépendance externe : on parse à la main avec strstr/sscanf.
 * Simple mais efficace pour du RSS bien formé.
 */

#include "newsbot.h"
#include <ctype.h>

/* ─── Utilitaires XML minimalistes ─── */

/* Extrait le contenu entre <tag>...</tag> (première occurrence) */
static int xml_extract(const char *src, const char *tag,
                        char *out, size_t out_size)
{
    char open_tag[128], close_tag[128];
    snprintf(open_tag,  sizeof(open_tag),  "<%s>",  tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    const char *start = strstr(src, open_tag);
    if (!start) return 0;
    start += strlen(open_tag);

    const char *end = strstr(start, close_tag);
    if (!end) return 0;

    size_t len = (size_t)(end - start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

/* Extrait un attribut : <tag attr="valeur"> */
static int xml_attr(const char *tag_str, const char *attr,
                    char *out, size_t out_size)
{
    char search[128];
    snprintf(search, sizeof(search), "%s=\"", attr);

    const char *p = strstr(tag_str, search);
    if (!p) return 0;
    p += strlen(search);

    const char *end = strchr(p, '"');
    if (!end) return 0;

    size_t len = (size_t)(end - p);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 1;
}

/* Décode les entités HTML basiques */
static void decode_entities(char *s)
{
    const struct { const char *ent; char rep; } table[] = {
        {"&amp;",  '&'}, {"&lt;",  '<'}, {"&gt;",  '>'},
        {"&quot;", '"'}, {"&apos;",'\''},{"&#39;", '\''},
        {NULL, 0}
    };

    for (int i = 0; table[i].ent; i++) {
        char *p;
        size_t elen = strlen(table[i].ent);
        while ((p = strstr(s, table[i].ent)) != NULL) {
            *p = table[i].rep;
            memmove(p + 1, p + elen, strlen(p + elen) + 1);
        }
    }
}

/* Supprime les balises CDATA */
static void strip_cdata(char *s)
{
    char *p;
    while ((p = strstr(s, "<![CDATA[")) != NULL) {
        size_t start = (size_t)(p - s);
        char *end = strstr(p, "]]>");
        if (!end) break;
        /* Retire <![CDATA[ ... ]]> en gardant le contenu */
        char *content = p + 9; /* après <![CDATA[ */
        size_t content_len = (size_t)(end - content);
        memmove(p, content, content_len);
        memmove(p + content_len, end + 3, strlen(end + 3) + 1);
    }
}

/* Convertit une date RFC 822 / ISO 8601 en time_t (approximatif) */
static time_t parse_date(const char *s)
{
    struct tm tm = {0};
    /* Essaie RFC 822 : "Mon, 01 Jan 2024 12:00:00 +0000" */
    if (strptime(s, "%a, %d %b %Y %H:%M:%S", &tm))
        return mktime(&tm);
    /* Essaie ISO 8601 : "2024-01-01T12:00:00Z" */
    if (strptime(s, "%Y-%m-%dT%H:%M:%S", &tm))
        return mktime(&tm);
    return time(NULL);
}

/* ─── Parse un flux RSS 2.0 ou Atom ─────────────────────────
 *
 * Retourne le nombre d'articles trouvés.
 */
int parse_rss(const char *xml, const char *source_name,
              Article *out, int max)
{
    if (!xml || !out || max <= 0) return 0;

    char buf[MAX_DESC_LEN];
    strncpy(buf, xml, sizeof(buf) - 1); /* copie locale pour strip */

    int count   = 0;
    int is_atom = (strstr(xml, "<feed") != NULL &&
                   strstr(xml, "xmlns=\"http://www.w3.org/2005/Atom\"") != NULL);

    const char *item_open  = is_atom ? "<entry>" : "<item>";
    const char *item_close = is_atom ? "</entry>" : "</item>";

    const char *cursor = xml;

    while (count < max) {
        const char *item_start = strstr(cursor, item_open);
        if (!item_start) break;

        const char *item_end = strstr(item_start, item_close);
        if (!item_end) break;

        /* Extrait le bloc de l'item */
        size_t block_len = (size_t)(item_end - item_start) + strlen(item_close);
        char *block = malloc(block_len + 1);
        if (!block) break;
        memcpy(block, item_start, block_len);
        block[block_len] = '\0';

        strip_cdata(block);

        Article *a = &out[count];
        memset(a, 0, sizeof(Article));
        strncpy(a->source_name, source_name, sizeof(a->source_name) - 1);

        /* Titre */
        if (!xml_extract(block, "title", a->title, sizeof(a->title)))
            strncpy(a->title, "(sans titre)", sizeof(a->title) - 1);
        decode_entities(a->title);

        /* URL */
        if (is_atom) {
            /* Atom : <link href="..." rel="alternate"/> */
            char *link_tag = strstr(block, "<link");
            if (link_tag) {
                char *tag_end = strchr(link_tag, '>');
                if (tag_end) {
                    char tag_buf[MAX_URL_LEN];
                    size_t tag_len = (size_t)(tag_end - link_tag);
                    if (tag_len < sizeof(tag_buf)) {
                        memcpy(tag_buf, link_tag, tag_len);
                        tag_buf[tag_len] = '\0';
                        xml_attr(tag_buf, "href", a->url, sizeof(a->url));
                    }
                }
            }
        } else {
            xml_extract(block, "link", a->url, sizeof(a->url));
        }

        /* Description */
        if (!xml_extract(block, "description", a->description, sizeof(a->description)))
            xml_extract(block, "summary",     a->description, sizeof(a->description));
        decode_entities(a->description);

        /* Date */
        char date_str[64] = "";
        if (!xml_extract(block, "pubDate", date_str, sizeof(date_str)))
            xml_extract(block, "published",  date_str, sizeof(date_str));
        a->published = parse_date(date_str);

        free(block);
        cursor = item_end + strlen(item_close);
        count++;
    }

    return count;
}

/* ─── Extrait les liens <a href> d'un HTML ───────────────── */
int parse_html_links(const char *html, const char *base_url,
                     char out[][MAX_URL_LEN], int max)
{
    if (!html || !out) return 0;

    int count = 0;
    const char *cursor = html;

    while (count < max) {
        /* Trouve le prochain <a */
        const char *tag = strstr(cursor, "<a ");
        if (!tag) tag  = strstr(cursor, "<A ");
        if (!tag) break;

        /* Cherche href= dans ce tag */
        const char *end_tag = strchr(tag, '>');
        if (!end_tag) { cursor = tag + 1; continue; }

        /* Isole le tag */
        size_t tag_len = (size_t)(end_tag - tag);
        if (tag_len >= MAX_URL_LEN) { cursor = end_tag; continue; }

        char tag_buf[MAX_URL_LEN];
        memcpy(tag_buf, tag, tag_len);
        tag_buf[tag_len] = '\0';

        char href[MAX_URL_LEN] = "";
        if (xml_attr(tag_buf, "href", href, sizeof(href))) {
            /* Ignore les ancres, javascript, mailto */
            if (href[0] != '#' &&
                strncmp(href, "javascript", 10) != 0 &&
                strncmp(href, "mailto",      6) != 0)
            {
                /* Résolution URL relative → absolue (simplifiée) */
                if (href[0] == '/') {
                    /* Extrait domaine du base_url */
                    char domain[256] = "";
                    const char *s = strstr(base_url, "://");
                    if (s) {
                        s += 3;
                        const char *slash = strchr(s, '/');
                        size_t dlen = slash ? (size_t)(slash - s) : strlen(s);
                        if (dlen < 200) {
                            char scheme[16] = "https";
                            memcpy(domain, base_url,
                                   (size_t)(strstr(base_url, "://") - base_url));
                            snprintf(out[count], MAX_URL_LEN, "%s://%.*s%s",
                                     scheme, (int)dlen, s, href);
                            count++;
                        }
                    }
                } else if (strncmp(href, "http", 4) == 0) {
                    strncpy(out[count], href, MAX_URL_LEN - 1);
                    count++;
                }
            }
        }

        cursor = end_tag;
    }

    return count;
}

/* ─── Extrait le texte brut d'un HTML (retire les balises) ── */
void extract_text_from_html(const char *html, char *out, size_t out_size)
{
    if (!html || !out) return;

    size_t j = 0;
    int in_tag = 0;

    for (size_t i = 0; html[i] && j < out_size - 1; i++) {
        if      (html[i] == '<') in_tag = 1;
        else if (html[i] == '>') in_tag = 0;
        else if (!in_tag) {
            out[j++] = html[i];
        }
    }
    out[j] = '\0';

    /* Compresse les espaces multiples */
    char *p = out;
    char *q = out;
    int space = 0;
    while (*p) {
        if (isspace((unsigned char)*p)) {
            if (!space) { *q++ = ' '; space = 1; }
        } else {
            *q++ = *p;
            space = 0;
        }
        p++;
    }
    *q = '\0';
}
